#include "Impl/RaytracingImpl.h"

#include "Buffers.h"
#include "Impl/Dx/d3dx12.h"
#include "RenderImpl.h"
#include "SparseArray.h"

#include <dxcapi.h>

// TODO https://developer.nvidia.com/blog/managing-memory-for-acceleration-structures-in-dxr/
// Shared buffer allocation strategy for BLAS
// https://intro-to-dxr.cwyman.org/presentations/IntroDXR_RaytracingAPI.pdf
// Compaction

namespace rl
{
struct AccelerationStructure
{
    ComPtr<ID3D12Resource> DxBuffer;
};

static_assert(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) == RaytracingInstanceDescSize);

struct BLAS : public AccelerationStructure
{
    // Strong refs so the source buffers outlive any build that reads them
    VertexBufferPtr VertexBuffer = {};
    StructuredBufferPtr StructuredVertexBuffer = {};
    IndexBufferPtr IndexBuffer = {};
    StructuredBufferPtr StructuredIndexBuffer = {};

    // One per sub-geometry, pGeometryDescs of the build desc points into this so it must not be resized after creation
    std::vector<D3D12_RAYTRACING_GEOMETRY_DESC> DxGeometryDescs;
};

struct TLAS : public AccelerationStructure
{
    // Geometries referenced by the most recent build, kept alive while the TLAS points at their BLAS
    std::vector<RaytracingGeometryPtr> BuiltGeometries;
};

struct Dx12ShaderTable
{
    Dx12StaticBufferAllocation RayGenShaderTable;
    Dx12StaticBufferAllocation MissShaderTable;
    Dx12StaticBufferAllocation HitGroupShaderTable;
	size_t RayGenShaderTableSize = 0;
    size_t MissShaderTableSize = 0;
    size_t MissShaderTableStride = 0;
	size_t HitGroupShaderTableSize = 0;
    size_t HitGroupShaderTableStride = 0;
};

SparseArray<BLAS, RaytracingGeometry_t> g_BLAS;
SparseArray<TLAS, RaytracingScene_t> g_TLAS;
SparseArray<Dx12ShaderTable, RaytracingShaderTable_t> g_ShaderTables;
SparseArray<ComPtr<ID3D12StateObject>, RaytracingPipelineState_t> g_RTPSOs;

bool CreateRaytracingGeometryImpl(RaytracingGeometry_t Handle, const RaytracingGeometryDesc& Desc)
{
    D3D12_GPU_VIRTUAL_ADDRESS VertexBufferGpuAddress = Dx12_GetVbAddress(Desc.VertexBuffer);
    if (VertexBufferGpuAddress == 0)
    {
        VertexBufferGpuAddress = Dx12_GetSbAddress(Desc.StructuredVertexBuffer);
        if (VertexBufferGpuAddress == 0)
        {
            return false;
        }
    }

    D3D12_GPU_VIRTUAL_ADDRESS IndexBufferGpuAddress = Dx12_GetIbAddress(Desc.IndexBuffer);
    if (IndexBufferGpuAddress == 0)
    {
        IndexBufferGpuAddress = Dx12_GetSbAddress(Desc.StructuredIndexBuffer);
        if (IndexBufferGpuAddress == 0)
        {
            return false;
        }
    }

    const uint32_t IndexSize = Desc.IndexFormat == RenderFormat::R32_UINT ? 4 : 2;

    BLAS& Geom = g_BLAS.Alloc(Handle);

    Geom.VertexBuffer = Desc.VertexBuffer;
    Geom.StructuredVertexBuffer = Desc.StructuredVertexBuffer;
    Geom.IndexBuffer = Desc.IndexBuffer;
    Geom.StructuredIndexBuffer = Desc.StructuredIndexBuffer;

    Geom.DxGeometryDescs.reserve(Desc.SubGeometries.size());
    for (const RaytracingSubGeometry& SubGeometry : Desc.SubGeometries)
    {
        D3D12_RAYTRACING_GEOMETRY_DESC& DxDesc = Geom.DxGeometryDescs.emplace_back();
        DxDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
        DxDesc.Triangles.VertexBuffer.StartAddress = VertexBufferGpuAddress;
        DxDesc.Triangles.VertexBuffer.StrideInBytes = Desc.VertexStride;
        DxDesc.Triangles.VertexCount = Desc.VertexCount;
        DxDesc.Triangles.VertexFormat = Dx12_Format(Desc.VertexFormat);
        DxDesc.Triangles.IndexBuffer = IndexBufferGpuAddress + static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(SubGeometry.IndexOffset) * IndexSize;
        DxDesc.Triangles.IndexFormat = Dx12_Format(Desc.IndexFormat);
        DxDesc.Triangles.IndexCount = SubGeometry.IndexCount;
        DxDesc.Triangles.Transform3x4 = 0;
        DxDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; // TODO: support transparent objects
    }

    return true;
}

bool CreateRaytracingSceneImpl(RaytracingScene_t RtScene)
{
    g_TLAS.Alloc(RtScene);

    return true;
}

bool CreateRaytracingPipelineStateImpl(RaytracingPipelineState_t RtPSO, const RaytracingPipelineStateDesc& Desc)
{
    CD3DX12_STATE_OBJECT_DESC StateObjectDesc(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);

    auto rootSignatureSubObject = StateObjectDesc.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
    rootSignatureSubObject->SetRootSignature(Dx12_GetRootSignature(Desc.RootSig != RootSignature_t::INVALID ? Desc.RootSig : g_render.RootSignature)); // TODO: Allow overrides on desc

    UINT MaxRayRecursion = static_cast<UINT>(Desc.MaxRayRecursion);
    auto configurationSubObject = StateObjectDesc.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
    configurationSubObject->Config(MaxRayRecursion);

    auto shaderConfigStateObject = StateObjectDesc.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
    shaderConfigStateObject->Config(8, 8);

    auto AddDxilLibary = [&](IDxcBlob* ShaderBlob, LPCWSTR ExportName)
    {
        assert(ShaderBlob && "Ray Shader is not valid for PSO compilation");

        CD3DX12_DXIL_LIBRARY_SUBOBJECT* LibSubObject = StateObjectDesc.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
        D3D12_SHADER_BYTECODE Shader = CD3DX12_SHADER_BYTECODE(ShaderBlob->GetBufferPointer(), ShaderBlob->GetBufferSize());
        LibSubObject->SetDXILLibrary(&Shader);
        LibSubObject->DefineExport(ExportName);
    };

    if (Desc.RayGenShader != RaytracingRayGenShader_t::INVALID)
    {
        AddDxilLibary(Dx12_GetRayGenShaderBlob(Desc.RayGenShader), L"RayGen");
    }

    if (Desc.MissShader != RaytracingMissShader_t::INVALID)
    {
        AddDxilLibary(Dx12_GetRayMissShaderBlob(Desc.MissShader), L"Miss");
    }

    ComPtr<ID3D12StateObject>& DxRTPSO = g_RTPSOs.Alloc(RtPSO);

    if (DXENSURE(g_render.DxDevice->CreateStateObject(StateObjectDesc, IID_PPV_ARGS(&DxRTPSO))))
    {
        if (!Desc.DebugName.empty())
            DxRTPSO->SetName(Desc.DebugName.c_str());

        return true;
    }

    return false;
}

bool CreateRaytracingShaderTableImpl(RaytracingShaderTable_t ShaderTable, RaytracingPipelineState_t RTPipelineState, const RaytracingShaderTableLayout& Layout)
{
    if (Layout.RayGenShader == RaytracingRayGenShader_t::INVALID)
    {
        OutputDebugStringA("Raygen shader required in shader table\n");
        return false;
    }

    if (Layout.MissShader == RaytracingMissShader_t::INVALID)
    {
        OutputDebugStringA("Miss shader required in shader table\n");
        return false;
    }       

    ID3D12StateObject* DxPipelineState = Dx12_GetRaytracingStateObject(RTPipelineState);
    if (!DxPipelineState)
        return false;

    ComPtr<ID3D12StateObjectProperties> StateObjectProperties = nullptr;
    if (!DXENSURE(DxPipelineState->QueryInterface(IID_PPV_ARGS(&StateObjectProperties))))
    {
		OutputDebugStringA("Failed to get state object properties for raytracing shader table creation\n");
		return false;
    }    

    Dx12ShaderTable& DxShaderTable = g_ShaderTables.Alloc(ShaderTable);

    std::vector<byte> HitGroupData;

    auto AddShaderRecord = [&](LPCWSTR ExportName)
    {
        void* ShaderIdentifierData = StateObjectProperties->GetShaderIdentifier(ExportName);
        size_t BufIt = HitGroupData.size();
        HitGroupData.resize(HitGroupData.size() + D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
        memcpy(&HitGroupData[BufIt], ShaderIdentifierData, D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
    };

    uint32_t RecordStride = 0;
    uint32_t CalculatedStride = 0;
    bool CalculateHitGroupStride = Layout.GetHitGroupStride() != 0;
    for (const RaytracingShaderRecord& Record : Layout.GetRecords())
    {
        if (Record.Type == RaytracingShaderRecordType::HITGROUP)
        {
            AddShaderRecord(L"HitGroup");
        }
        else if (Record.Type == RaytracingShaderRecordType::DATA)
        {
            size_t BufIt = HitGroupData.size();
            HitGroupData.resize(HitGroupData.size() + sizeof(RaytracingShaderRecordData));
            memcpy(&HitGroupData[BufIt], Record.Data.Data, sizeof(RaytracingShaderRecordData));
        }        

        if (CalculateHitGroupStride && CalculatedStride == 0)
        {
            RecordStride++;
            if (RecordStride == Layout.GetHitGroupStride())
            {
                CalculatedStride = static_cast<uint32_t>(HitGroupData.size());
            }
        }        
    }
    // TODO: Pull shader identifier from the shader object if necessary
    DxShaderTable.RayGenShaderTable = Dx12_CreateByteBuffer(StateObjectProperties->GetShaderIdentifier(L"RayGen"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    DxShaderTable.RayGenShaderTableSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    DxShaderTable.MissShaderTable = Dx12_CreateByteBuffer(StateObjectProperties->GetShaderIdentifier(L"Miss"), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES, D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
    DxShaderTable.MissShaderTableSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    DxShaderTable.MissShaderTableStride = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES; // 1 Entry

    if (!HitGroupData.empty())
    {
        DxShaderTable.HitGroupShaderTable = Dx12_CreateByteBuffer(HitGroupData.data(), HitGroupData.size(), D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT);
        DxShaderTable.HitGroupShaderTableSize = HitGroupData.size();
        DxShaderTable.HitGroupShaderTableStride = CalculatedStride;
    }

    return true;
}

void DestroyRaytracingGeometryImpl(RaytracingGeometry_t RtGeometry)
{
    if (BLAS* Geom = g_BLAS.Get(RtGeometry))
    {
        Dx12_DeferRelease(std::move(Geom->DxBuffer));
    }

    g_BLAS.Free(RtGeometry);
}

void DestroyRaytracingSceneImpl(RaytracingScene_t RtScene)
{
    if (TLAS* Scene = g_TLAS.Get(RtScene))
    {
        Dx12_DeferRelease(std::move(Scene->DxBuffer));
    }

    g_TLAS.Free(RtScene);
}

void DestroyRaytracingPipelineStateImpl(RaytracingPipelineState_t RTPipelineState)
{
    g_RTPSOs.Free(RTPipelineState);
}

void DestroyRaytracingShaderTableImpl(RaytracingShaderTable_t RTShaderTable)
{
	g_ShaderTables.Free(RTShaderTable);
}

ID3D12StateObject* Dx12_GetRaytracingStateObject(RaytracingPipelineState_t RaytracingPipelineState)
{
    return g_RTPSOs.Valid(RaytracingPipelineState) ? g_RTPSOs[RaytracingPipelineState].Get() : nullptr;
}

// TODO: There should be one shader table that internally stores these offsets.
D3D12_DISPATCH_RAYS_DESC Dx12_GetDispatchRaysDesc(RaytracingShaderTable_t ShaderTable, uint32_t X, uint32_t Y, uint32_t Z)
{
    const Dx12ShaderTable* const DxShaderTable = g_ShaderTables.Get(ShaderTable);
    if (!DxShaderTable)
        return {};

    D3D12_DISPATCH_RAYS_DESC DxRayDesc = {};

    DxRayDesc.RayGenerationShaderRecord.StartAddress = Dx12_GetByteBufferAddress(DxShaderTable->RayGenShaderTable);
    DxRayDesc.RayGenerationShaderRecord.SizeInBytes = DxShaderTable->RayGenShaderTableSize;

    DxRayDesc.MissShaderTable.StartAddress = Dx12_GetByteBufferAddress(DxShaderTable->MissShaderTable);
    DxRayDesc.MissShaderTable.SizeInBytes = DxShaderTable->MissShaderTableSize;
    DxRayDesc.MissShaderTable.StrideInBytes = DxShaderTable->MissShaderTableStride;

    DxRayDesc.HitGroupTable.StartAddress = Dx12_GetByteBufferAddress(DxShaderTable->HitGroupShaderTable);
    DxRayDesc.HitGroupTable.SizeInBytes = DxShaderTable->HitGroupShaderTableSize;
    DxRayDesc.HitGroupTable.StrideInBytes = DxShaderTable->HitGroupShaderTableStride;

    DxRayDesc.Width = X;
    DxRayDesc.Height = Y;
    DxRayDesc.Depth = Z;

    return DxRayDesc;
}

ID3D12StateObject* Dx12_GetPipelineState(RaytracingPipelineState_t pso)
{
	return g_RTPSOs.Valid(pso) ? g_RTPSOs[pso].Get() : nullptr;
}

ID3D12Resource* Dx12_GetRaytracingScene(RaytracingScene_t scene)
{
	return g_TLAS.Valid(scene) ? g_TLAS[scene].DxBuffer.Get() : nullptr;
}
}
