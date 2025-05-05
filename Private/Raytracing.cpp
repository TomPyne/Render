#include "Raytracing.h"

#include "Buffers.h"
#include "IDArray.h"
#include "Impl/RaytracingImpl.h"

namespace rl
{

enum class RaytracingShaderRecordType : uint32_t
{
    RAYGEN,
    MISS,
    HITGROUP,
    DATA,
};

struct RaytracingShaderRecordRayGenShader
{
    RaytracingRayGenShader_t RayGenShader = {};
};

struct RaytracingShaderRecordMissShader
{
    RaytracingMissShader_t MissShader = {};
};

struct RaytracingShaderRecordHitGroup
{
    RaytracingAnyHitShader_t AnyHitShader = {};
    RaytracingClosestHitShader_t ClosestHitShader = {};
};

struct RaytracingShaderRecordData
{
    uint32_t Data[4];
};

struct RaytracingShaderRecord
{
    RaytracingShaderRecordType Type;
    union
    {
        RaytracingShaderRecordRayGenShader RayGenShader;
        RaytracingShaderRecordMissShader MissShader;
        RaytracingShaderRecordHitGroup HitGroup;
        RaytracingShaderRecordData Data;
    };

    RaytracingShaderRecord(RaytracingShaderRecordType InType)
        : Type(InType)
    {
    }
};

struct RaytracingSceneData
{
    std::vector<RaytracingGeometryPtr> Geometry;

    bool NeedsUpdate = false;
};

IDArray<RaytracingGeometry_t, RaytracingGeometryDesc> g_RaytracingGeometry;
IDArray<RaytracingScene_t, RaytracingSceneData> g_RaytracingScenes;

IDArray<RaytracingPipelineState_t, RaytracingPipelineStateDesc> g_RaytracingPipelines;

IDArray<RaytracingShaderTable_t, RaytracingShaderTableLayout> g_RaytracingShaderTables;

RaytracingGeometry_t CreateRaytracingGeometry(const RaytracingGeometryDesc& Desc)
{
    if (Desc.IndexFormat != RenderFormat::R32_UINT && Desc.IndexFormat != RenderFormat::R16_UINT)
    {
        return RaytracingGeometry_t::INVALID;
    }

    // Must only specify one or the other
    if (Desc.VertexBuffer == VertexBuffer_t::INVALID)
    {
        if (Desc.StructuredVertexBuffer == StructuredBuffer_t::INVALID)
        {
            return RaytracingGeometry_t::INVALID;
        }
    }
    else
    {
        if (Desc.StructuredVertexBuffer != StructuredBuffer_t::INVALID)
        {
            return RaytracingGeometry_t::INVALID;
        }
    }

    if (Desc.IndexBuffer != IndexBuffer_t::INVALID)
    {
        if (Desc.StructuredVertexBuffer != StructuredBuffer_t::INVALID)
        {
            return RaytracingGeometry_t::INVALID;
        }
    }

    RaytracingGeometry_t Handle = g_RaytracingGeometry.Create(Desc);

    if (!CreateRaytracingGeometryImpl(Handle, Desc))
    {
        g_RaytracingGeometry.Release(Handle);
        return RaytracingGeometry_t::INVALID;
    }

    return Handle;
}

RaytracingScene_t CreateRaytracingScene()
{
    RaytracingScene_t Handle = g_RaytracingScenes.Create();

    if (!CreateRaytracingSceneImpl(Handle))
    {
        g_RaytracingScenes.Release(Handle);
        return RaytracingScene_t::INVALID;
    }

    return Handle;
}

RaytracingPipelineState_t CreateRaytracingPipelineState(const RaytracingPipelineStateDesc& Desc)
{
    RaytracingPipelineState_t Handle = g_RaytracingPipelines.Create(Desc);

    if (!CreateRaytracingPipelineStateImpl(Handle, Desc))
    {
        g_RaytracingPipelines.Release(Handle);
        return RaytracingPipelineState_t::INVALID;
    }

    return Handle;
}

RaytracingShaderTable_t CreateRaytracingShaderTable(const RaytracingShaderTableLayout& Layout)
{
    return RaytracingShaderTable_t();
}

void AddRaytracingGeometryToScene(RaytracingGeometry_t Geometry, RaytracingScene_t Scene)
{
    if (Geometry != RaytracingGeometry_t::INVALID && Scene != RaytracingScene_t::INVALID)
    {
        AddRaytracingGeometryToSceneImpl(Geometry, Scene);
    }
}

void RemoveRaytracingGeometryFromScene(RaytracingGeometry_t Geometry, RaytracingScene_t Scene)
{
    if (Geometry != RaytracingGeometry_t::INVALID && Scene != RaytracingScene_t::INVALID)
    {
        RemoveRaytracingGeometryFromSceneImpl(Geometry, Scene);
    }
}

void RenderRef(RaytracingGeometry_t geometry)
{
    g_RaytracingGeometry.AddRef(geometry);
}

void RenderRef(RaytracingScene_t scene)
{
    g_RaytracingScenes.AddRef(scene);
}

void RenderRef(RaytracingPipelineState_t RTPipelineState)
{
    g_RaytracingPipelines.AddRef(RTPipelineState);
}

void RenderRelease(RaytracingGeometry_t geometry)
{
    if (g_RaytracingGeometry.Release(geometry))
    {
        DestroyRaytracingGeometryImpl(geometry);
    }
}

void RenderRelease(RaytracingScene_t scene)
{
    if (g_RaytracingScenes.Release(scene))
    {
        DestroyRaytracingSceneImpl(scene);
    }
}

void RenderRelease(RaytracingPipelineState_t RTPipelineState)
{
    if (g_RaytracingPipelines.Release(RTPipelineState))
    {
        DestroyRaytracingPipelineStateImpl(RTPipelineState);
    }
}

void RaytracingShaderTableLayout::AddRayGenShader(RaytracingRayGenShader_t RayGenShader)
{
    RaytracingShaderRecord Record(RaytracingShaderRecordType::RAYGEN);
    Record.RayGenShader.RayGenShader = RayGenShader;
    Records.push_back(Record);
}

void RaytracingShaderTableLayout::AddMissShader(RaytracingMissShader_t MissShader)
{
    RaytracingShaderRecord Record(RaytracingShaderRecordType::MISS);
    Record.MissShader.MissShader = MissShader;
    Records.push_back(Record);
}

void RaytracingShaderTableLayout::AddHitGroup(RaytracingAnyHitShader_t AnyHitShader, RaytracingClosestHitShader_t ClosestHitShader, uint8_t* Data, size_t DataSize)
{
    size_t NumRecords = (DataSize + 15) / 16;

    if (HitGroupStride > 0 && HitGroupStride != (NumRecords + 1))
    {
        OutputDebugStringA("Trying to add hit group with a different stride to the previously set ones");
        return;
    }
    else
    {
        HitGroupStride = static_cast<uint32_t>(NumRecords + 1); // Num data records + the shader record
    }

    RaytracingShaderRecord Record(RaytracingShaderRecordType::HITGROUP);
    Record.HitGroup.AnyHitShader = AnyHitShader;
    Record.HitGroup.ClosestHitShader = ClosestHitShader;
    Records.push_back(Record);

    for (uint32_t RecordIt = 0; RecordIt < NumRecords; RecordIt++, Data += 4, DataSize -= 4)
    {
        RaytracingShaderRecord Record(RaytracingShaderRecordType::DATA);
        memcpy(Record.Data.Data, Data, DataSize < 4 ? DataSize : 4);
        Records.push_back(Record);
    }
}

}
