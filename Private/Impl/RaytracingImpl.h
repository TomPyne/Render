#pragma once

#include "Raytracing.h"

namespace rl
{
bool CreateRaytracingGeometryImpl(RaytracingGeometry_t Handle, const RaytracingGeometryDesc& Desc);

bool CreateRaytracingSceneImpl(RaytracingScene_t RtScene);

bool CreateRaytracingPipelineStateImpl(RaytracingPipelineState_t RtPSO, const RaytracingPipelineStateDesc& Desc);

void AddRaytracingGeometryToSceneImpl(RaytracingGeometry_t Geometry, RaytracingScene_t Scene);
void RemoveRaytracingGeometryFromSceneImpl(RaytracingGeometry_t Geometry, RaytracingScene_t Scene);

void DestroyRaytracingGeometryImpl(RaytracingGeometry_t RtGeometry);
void DestroyRaytracingSceneImpl(RaytracingScene_t RtScene);
void DestroyRaytracingPipelineStateImpl(RaytracingPipelineState_t RTPipelineState);
}