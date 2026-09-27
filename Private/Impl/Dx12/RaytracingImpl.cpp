#include "Impl/RaytracingImpl.h"

#include "Buffers.h"
#include "Impl/Dx/d3dx12.h"
#include "RenderImpl.h"
#include "SparseArray.h"

#include <dxcapi.h>

#include <algorithm>
#include <atomic>
#include <cassert>

// TODO https://developer.nvidia.com/blog/managing-memory-for-acceleration-structures-in-dxr/
// Shared buffer allocation strategy for BLAS
// https://intro-to-dxr.cwyman.org/presentations/IntroDXR_RaytracingAPI.pdf
// Compaction

namespace rl
{
struct AccelerationStructure
{
    ComPtr<ID3D12Resource> DxBuffer;

    // Filled by the prepare, read by the CommandList build. Only valid when PreparedFrame is the current frame.
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC DxBuildDesc = {};
    uint64_t PreparedFrame = 0;

    // Set when the build is recorded. Written at replay, which the main thread waits for.
    bool Built = false;
};

static_assert(sizeof(D3D12_RAYTRACING_INSTANCE_DESC) == RaytracingInstanceDescSize);

struct BLAS : public AccelerationStructure
{
    // Strong refs so the source buffers outlive any build that reads them
    VertexBufferPtr VertexBuffer = {};
    StructuredBufferPtr StructuredVertexBuffer = {};
    IndexBufferPtr IndexBuffer = {};
    StructuredBufferPtr StructuredIndexBuffer = {};

    // One per sub-geometry. pGeometryDescs is set from this at record time, as g_BLAS can reallocate after a prepare.
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

// TODO RT ASYNC: persistent scratch assumes single in-order queue; needs per-frame-in-flight scratch if builds move to async compute
struct Dx12RaytracingScratch
{
    ComPtr<ID3D12Resource> DxBuffer;
    uint64_t Size = 0;
    uint64_t Offset = 0;
    uint64_t FrameTotal = 0;   // Reserved this frame across every buffer, sizes the next grow
};

Dx12RaytracingScratch g_Scratch;

D3D12_GPU_VIRTUAL_ADDRESS Dx12_ReserveRaytracingScratch(uint64_t Size)
{
    const uint64_t Alignment = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BYTE_ALIGNMENT;
    const uint64_t AlignedSize = (Size + Alignment - 1) & ~(Alignment - 1);

    if (!g_Scratch.DxBuffer || g_Scratch.Offset + AlignedSize > g_Scratch.Size)
    {
        // Ranges already reserved this frame keep pointing into the old buffer, the deferred release keeps it alive until they've executed
        Dx12_DeferRelease(std::move(g_Scratch.DxBuffer));

        g_Scratch.Size = (std::max)({ g_Scratch.Size * 2, g_Scratch.FrameTotal + AlignedSize, Alignment });
        g_Scratch.Offset = 0;
        g_Scratch.DxBuffer = Dx12_CreateBuffer(g_Scratch.Size, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        g_Scratch.DxBuffer->SetName(L"Raytracing Scratch");
    }

    const D3D12_GPU_VIRTUAL_ADDRESS Address = g_Scratch.DxBuffer->GetGPUVirtualAddress() + g_Scratch.Offset;

    g_Scratch.Offset += AlignedSize;
    g_Scratch.FrameTotal += AlignedSize;

    return Address;
}

void Dx12_RaytracingBeginFrame()
{
    // TODO RT ASYNC: persistent scratch assumes single in-order queue; needs per-frame-in-flight scratch if builds move to async compute
    g_Scratch.Offset = 0;
    g_Scratch.FrameTotal = 0;
}

static void Dx12_LogRaytracingBuildSkipped(const char* Message)
{
    static std::atomic<bool> Logged = false;

    if (!Logged.exchange(true))
    {
        OutputDebugStringA(Message);
    }
}

D3D12_RAYTRACING_INSTANCE_FLAGS Dx12_RaytracingInstanceFlags(RaytracingInstanceFlags Flags)
{
    D3D12_RAYTRACING_INSTANCE_FLAGS DxFlags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

    if (HasEnumFlags(Flags, RaytracingInstanceFlags::TRIANGLE_CULL_DISABLE))
        DxFlags |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_CULL_DISABLE;
    if (HasEnumFlags(Flags, RaytracingInstanceFlags::TRIANGLE_FRONT_COUNTERCLOCKWISE))
        DxFlags |= D3D12_RAYTRACING_INSTANCE_FLAG_TRIANGLE_FRONT_COUNTERCLOCKWISE;
    if (HasEnumFlags(Flags, RaytracingInstanceFlags::FORCE_OPAQUE))
        DxFlags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_OPAQUE;
    if (HasEnumFlags(Flags, RaytracingInstanceFlags::FORCE_NON_OPAQUE))
        DxFlags |= D3D12_RAYTRACING_INSTANCE_FLAG_FORCE_NON_OPAQUE;

    return DxFlags;
}

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

    Geom.VertexBuffer = VertexBufferPtr::Ref(Desc.VertexBuffer);
    Geom.StructuredVertexBuffer = StructuredBufferPtr::Ref(Desc.StructuredVertexBuffer);
    Geom.IndexBuffer = IndexBufferPtr::Ref(Desc.IndexBuffer);
    Geom.StructuredIndexBuffer = StructuredBufferPtr::Ref(Desc.StructuredIndexBuffer);

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

bool PrepareRaytracingGeometryBuildImpl(RaytracingGeometry_t Geometry)
{
    BLAS* Geom = g_BLAS.Get(Geometry);
    if (!Geom || Geom->DxGeometryDescs.empty())
        return false;

    assert(Geom->PreparedFrame != g_render.FrameIndex && "Raytracing geometry prepared twice in one frame");

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& BuildDesc = Geom->DxBuildDesc;
    BuildDesc = {};
    BuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    BuildDesc.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    BuildDesc.Inputs.NumDescs = static_cast<UINT>(Geom->DxGeometryDescs.size());
    BuildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    BuildDesc.Inputs.pGeometryDescs = Geom->DxGeometryDescs.data();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};
    g_render.DxDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BuildDesc.Inputs, &PrebuildInfo);

    // BLAS rebuilds are rare, so always build into a new buffer rather than one an in-flight TLAS may reference
    Dx12_DeferRelease(std::move(Geom->DxBuffer));
    Geom->DxBuffer = Dx12_CreateBuffer(PrebuildInfo.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    Geom->DxBuffer->SetName(L"BLAS");

    BuildDesc.DestAccelerationStructureData = Geom->DxBuffer->GetGPUVirtualAddress();
    BuildDesc.ScratchAccelerationStructureData = Dx12_ReserveRaytracingScratch(PrebuildInfo.ScratchDataSizeInBytes);
    BuildDesc.Inputs.pGeometryDescs = nullptr;

    Geom->PreparedFrame = g_render.FrameIndex;
    Geom->Built = false;

    return true;
}

bool PrepareRaytracingSceneBuildImpl(RaytracingScene_t Scene, uint32_t InstanceCount)
{
    TLAS* SceneAS = g_TLAS.Get(Scene);
    if (!SceneAS)
        return false;

    assert(SceneAS->PreparedFrame != g_render.FrameIndex && "Raytracing scene prepared twice in one frame");

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC& BuildDesc = SceneAS->DxBuildDesc;
    BuildDesc = {};
    BuildDesc.Inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    BuildDesc.Inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    BuildDesc.Inputs.NumDescs = InstanceCount;
    BuildDesc.Inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO PrebuildInfo = {};
    g_render.DxDevice->GetRaytracingAccelerationStructurePrebuildInfo(&BuildDesc.Inputs, &PrebuildInfo);

    // TODO RT ASYNC: reusing the result buffer assumes a single in-order queue, previous frames' traces finish before this build
    if (!SceneAS->DxBuffer || SceneAS->DxBuffer->GetDesc().Width < PrebuildInfo.ResultDataMaxSizeInBytes)
    {
        Dx12_DeferRelease(std::move(SceneAS->DxBuffer));
        SceneAS->DxBuffer = Dx12_CreateBuffer(PrebuildInfo.ResultDataMaxSizeInBytes, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
        SceneAS->DxBuffer->SetName(L"TLAS");
    }

    BuildDesc.DestAccelerationStructureData = SceneAS->DxBuffer->GetGPUVirtualAddress();
    BuildDesc.ScratchAccelerationStructureData = Dx12_ReserveRaytracingScratch(PrebuildInfo.ScratchDataSizeInBytes);

    SceneAS->PreparedFrame = g_render.FrameIndex;
    SceneAS->Built = false;

    return true;
}

void WriteRaytracingInstancesImpl(RaytracingScene_t Scene, void* Dst, const RaytracingInstance* Src, uint32_t Count)
{
    TLAS* SceneAS = g_TLAS.Get(Scene);
    if (!SceneAS)
        return;

    if (SceneAS->PreparedFrame != g_render.FrameIndex || SceneAS->DxBuildDesc.Inputs.NumDescs != Count)
    {
        assert(0 && "WriteRaytracingInstances must follow PrepareRaytracingSceneBuild with the same count");
        Dx12_LogRaytracingBuildSkipped("WriteRaytracingInstances without a matching prepare, the scene build will be skipped\n");

        SceneAS->PreparedFrame = 0;
        return;
    }

    assert((reinterpret_cast<uintptr_t>(Dst) & (D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT - 1)) == 0 && "Raytracing instances must be 16 byte aligned");

    std::vector<RaytracingGeometryPtr> BuiltGeometries;
    BuiltGeometries.reserve(Count);

    D3D12_RAYTRACING_INSTANCE_DESC* DxDst = static_cast<D3D12_RAYTRACING_INSTANCE_DESC*>(Dst);

    for (uint32_t InstanceIt = 0; InstanceIt < Count; InstanceIt++)
    {
        const RaytracingInstance& Instance = Src[InstanceIt];

        D3D12_RAYTRACING_INSTANCE_DESC DxInstance = {};
        memcpy(DxInstance.Transform, Instance.Transform, sizeof(DxInstance.Transform));
        DxInstance.InstanceID = Instance.InstanceID & 0xFFFFFF;
        DxInstance.InstanceMask = Instance.Mask;
        DxInstance.InstanceContributionToHitGroupIndex = 0;
        DxInstance.Flags = Dx12_RaytracingInstanceFlags(Instance.Flags);

        const BLAS* Geom = g_BLAS.Get(Instance.Geometry);
        const bool Buildable = Geom && Geom->DxBuffer && (Geom->Built || Geom->PreparedFrame == g_render.FrameIndex);
        assert(Buildable && "Raytracing instance geometry hasn't been built or prepared this frame");

        // A null acceleration structure makes the instance inactive
        if (Buildable)
        {
            DxInstance.AccelerationStructure = Geom->DxBuffer->GetGPUVirtualAddress();

            BuiltGeometries.push_back(RaytracingGeometryPtr::Ref(Instance.Geometry));
        }

        // Dst may be write-combined, so write each instance once and never read it back
        memcpy(&DxDst[InstanceIt], &DxInstance, sizeof(DxInstance));
    }

    SceneAS->BuiltGeometries = std::move(BuiltGeometries);
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

void Dx12_BuildRaytracingGeometry(ID3D12GraphicsCommandList4* DxCl, const RaytracingGeometry_t* Geometries, uint32_t Count)
{
    const D3D12_RESOURCE_BARRIER UavBarrier = Dx12_UavBarrier(nullptr);

    // Scratch ranges are reused across frames, order them after the previous frame's builds
    DxCl->ResourceBarrier(1u, &UavBarrier);

    for (uint32_t GeomIt = 0; GeomIt < Count; GeomIt++)
    {
        BLAS* Geom = g_BLAS.Get(Geometries[GeomIt]);
        if (!Geom || Geom->PreparedFrame != g_render.FrameIndex)
        {
            assert(0 && "BuildRaytracingGeometry on geometry that wasn't prepared this frame");
            Dx12_LogRaytracingBuildSkipped("BuildRaytracingGeometry on geometry that wasn't prepared this frame, skipped\n");
            continue;
        }

        D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BuildDesc = Geom->DxBuildDesc;
        BuildDesc.Inputs.pGeometryDescs = Geom->DxGeometryDescs.data();

        DxCl->BuildRaytracingAccelerationStructure(&BuildDesc, 0, nullptr);

        Geom->Built = true;
    }

    DxCl->ResourceBarrier(1u, &UavBarrier);
}

void Dx12_BuildRaytracingScene(ID3D12GraphicsCommandList4* DxCl, RaytracingScene_t Scene, GPUAddress_t InstanceDescs, uint32_t InstanceCount)
{
    TLAS* SceneAS = g_TLAS.Get(Scene);
    if (!SceneAS || SceneAS->PreparedFrame != g_render.FrameIndex || SceneAS->DxBuildDesc.Inputs.NumDescs != InstanceCount)
    {
        assert(0 && "BuildRaytracingScene without a matching prepare this frame");
        Dx12_LogRaytracingBuildSkipped("BuildRaytracingScene without a matching prepare this frame, skipped\n");
        return;
    }

    assert((static_cast<uint64_t>(InstanceDescs) & (D3D12_RAYTRACING_INSTANCE_DESCS_BYTE_ALIGNMENT - 1)) == 0 && "Raytracing instances must be 16 byte aligned");

    const D3D12_RESOURCE_BARRIER UavBarrier = Dx12_UavBarrier(nullptr);

    // Scratch ranges are reused across frames, order them after the previous frame's builds
    DxCl->ResourceBarrier(1u, &UavBarrier);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC BuildDesc = SceneAS->DxBuildDesc;
    BuildDesc.Inputs.InstanceDescs = static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(InstanceDescs);

    DxCl->BuildRaytracingAccelerationStructure(&BuildDesc, 0, nullptr);

    // So ray dispatches can read the scene
    DxCl->ResourceBarrier(1u, &UavBarrier);

    SceneAS->Built = true;
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
