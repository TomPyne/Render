#pragma once

#include "RenderTypes.h"
#include "Shaders.h"

namespace rl
{
// Geometry
RENDER_TYPE(RaytracingGeometry_t);
RENDER_TYPE(RaytracingScene_t);

// RTPSO
RENDER_TYPE(RaytracingPipelineState_t);

RENDER_TYPE(RaytracingShaderTable_t);

struct RaytracingShaderRecord;
struct RaytracingShaderTableLayout
{
	void AddRayGenShader(RaytracingRayGenShader_t RayGenShader);
	void AddMissShader(RaytracingMissShader_t MissShader);
	void AddHitGroup(RaytracingAnyHitShader_t AnyHitShader, RaytracingClosestHitShader_t ClosestHitShader, uint8_t* Data, size_t DataSize);

	const std::vector<RaytracingShaderRecord>& GetRecords() const noexcept { return Records; }
	uint32_t GetHitGroupStride() const noexcept { return HitGroupStride; }
private:
	std::vector<RaytracingShaderRecord> Records;
	uint32_t HitGroupStride = 0;
};

struct RaytracingPipelineStateDesc
{
	RaytracingRayGenShader_t RayGenShader = {};
	RaytracingMissShader_t MissShader = {};
	RaytracingAnyHitShader_t AnyHitShader = {};
	RaytracingClosestHitShader_t ClosestHitShader = {};
	uint32_t MaxRayRecursion = 2;

	std::wstring DebugName;
};

struct RaytracingGeometryDesc
{
	// Must supply exactly one of either vertex or structured buffer
	VertexBuffer_t VertexBuffer = {};
	StructuredBuffer_t StructuredVertexBuffer = {};

	// Required
	RenderFormat VertexFormat = RenderFormat::UNKNOWN;
	uint32_t VertexCount = 0;
	uint32_t VertexStride = 0;

	// Optional, supply either index or structured buffer
	IndexBuffer_t IndexBuffer = {};
	StructuredBuffer_t StructuredIndexBuffer = {};
	RenderFormat IndexFormat = RenderFormat::UNKNOWN;
	uint32_t IndexCount = 0;
	uint32_t IndexOffset = 0;
};

RaytracingGeometry_t CreateRaytracingGeometry(const RaytracingGeometryDesc& Desc);

RaytracingScene_t CreateRaytracingScene();

RaytracingPipelineState_t CreateRaytracingPipelineState(const RaytracingPipelineStateDesc& Desc);

RaytracingShaderTable_t CreateRaytracingShaderTable(const RaytracingShaderTableLayout& Layout);

// Perhaps return an geometry index from here to assist with creating shader tables
void AddRaytracingGeometryToScene(RaytracingGeometry_t Geometry, RaytracingScene_t Scene);
void RemoveRaytracingGeometryFromScene(RaytracingGeometry_t Geometry, RaytracingScene_t Scene);

void RenderRef(RaytracingGeometry_t geometry);
void RenderRef(RaytracingScene_t scene);
void RenderRef(RaytracingPipelineState_t RTPipelineState);

void RenderRelease(RaytracingGeometry_t geometry);
void RenderRelease(RaytracingScene_t scene);
void RenderRelease(RaytracingPipelineState_t RTPipelineState);

}