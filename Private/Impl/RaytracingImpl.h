#pragma once

#include "Raytracing.h"

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

bool CreateRaytracingGeometryImpl(RaytracingGeometry_t Handle, const RaytracingGeometryDesc& Desc);

bool CreateRaytracingSceneImpl(RaytracingScene_t RtScene);

bool CreateRaytracingPipelineStateImpl(RaytracingPipelineState_t RtPSO, const RaytracingPipelineStateDesc& Desc);

bool CreateRaytracingShaderTableImpl(RaytracingShaderTable_t ShaderTable, RaytracingPipelineState_t RTPipelineState, const RaytracingShaderTableLayout& Layout);

void AddRaytracingGeometryToSceneImpl(RaytracingGeometry_t Geometry, RaytracingScene_t Scene);
void RemoveRaytracingGeometryFromSceneImpl(RaytracingGeometry_t Geometry, RaytracingScene_t Scene);

// Blocking build, flushes GPU.
void BuildRaytracingSceneImpl(RaytracingScene_t Scene);

void DestroyRaytracingGeometryImpl(RaytracingGeometry_t RtGeometry);
void DestroyRaytracingSceneImpl(RaytracingScene_t RtScene);
void DestroyRaytracingPipelineStateImpl(RaytracingPipelineState_t RTPipelineState);
}