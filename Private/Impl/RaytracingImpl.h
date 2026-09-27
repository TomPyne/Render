#pragma once

#include "Raytracing.h"

namespace rl
{


bool CreateRaytracingGeometryImpl(RaytracingGeometry_t Handle, const RaytracingGeometryDesc& Desc);

bool CreateRaytracingSceneImpl(RaytracingScene_t RtScene);

bool CreateRaytracingPipelineStateImpl(RaytracingPipelineState_t RtPSO, const RaytracingPipelineStateDesc& Desc);

bool CreateRaytracingShaderTableImpl(RaytracingShaderTable_t ShaderTable, RaytracingPipelineState_t RTPipelineState, const RaytracingShaderTableLayout& Layout);

void DestroyRaytracingGeometryImpl(RaytracingGeometry_t RtGeometry);
void DestroyRaytracingSceneImpl(RaytracingScene_t RtScene);
void DestroyRaytracingPipelineStateImpl(RaytracingPipelineState_t RTPipelineState);
void DestroyRaytracingShaderTableImpl(RaytracingShaderTable_t RTShaderTable);
}