#pragma once

#include "RenderTypes.h"
#include "RootSignature.h"
#include "Shaders.h"

namespace rl
{
enum class RaytracingShaderRecordType : uint32_t
{
	HITGROUP,
	DATA,
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
		RaytracingShaderRecordHitGroup HitGroup;
		RaytracingShaderRecordData Data;
	};

	RaytracingShaderRecord(RaytracingShaderRecordType InType)
		: Type(InType)
	{
	}
};

struct RaytracingShaderTableLayout
{
	~RaytracingShaderTableLayout();

	void AddHitGroup(RaytracingAnyHitShader_t AnyHitShader, RaytracingClosestHitShader_t ClosestHitShader, uint8_t* Data, size_t DataSize);

	const std::vector<RaytracingShaderRecord>& GetRecords() const noexcept { return Records; }
	uint32_t GetHitGroupStride() const noexcept { return HitGroupStride; }

	RaytracingRayGenShader_t RayGenShader = {};
	RaytracingMissShader_t MissShader = {};
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
	RootSignature_t RootSig = RootSignature_t::INVALID;

	std::wstring DebugName;
};

// A range of the geometry's index buffer. Indices are absolute into the vertex buffer.
struct RaytracingSubGeometry
{
	uint32_t IndexOffset = 0;
	uint32_t IndexCount = 0;
};

struct RaytracingGeometryDesc
{
	// Must supply exactly one of either vertex or structured buffer
	VertexBuffer_t VertexBuffer = {};
	StructuredBuffer_t StructuredVertexBuffer = {};

	RenderFormat VertexFormat = RenderFormat::UNKNOWN;
	uint32_t VertexCount = 0;
	uint32_t VertexStride = 0;

	// Must supply exactly one of either index or structured buffer
	IndexBuffer_t IndexBuffer = {};
	StructuredBuffer_t StructuredIndexBuffer = {};
	RenderFormat IndexFormat = RenderFormat::UNKNOWN;

	// At least one. GeometryIndex() in hit shaders is the index into this array.
	std::vector<RaytracingSubGeometry> SubGeometries;
};

enum class RaytracingInstanceFlags : uint8_t
{
	NONE = 0,
	TRIANGLE_CULL_DISABLE = 1 << 0,
	TRIANGLE_FRONT_COUNTERCLOCKWISE = 1 << 1,
	FORCE_OPAQUE = 1 << 2,
	FORCE_NON_OPAQUE = 1 << 3,
};
IMPLEMENT_FLAGS(RaytracingInstanceFlags, uint8_t);

struct RaytracingInstance
{
	RaytracingGeometry_t Geometry = {};
	float Transform[3][4] = {};		// Object to world, row-major 3x4 affine, applied to column vectors
	uint32_t InstanceID = 0;		// 24 bits
	uint8_t Mask = 0xFF;
	RaytracingInstanceFlags Flags = RaytracingInstanceFlags::NONE;
};

static constexpr uint32_t RaytracingInstanceDescSize = 64; // Bytes per packed instance written by WriteRaytracingInstances

RaytracingGeometry_t CreateRaytracingGeometry(const RaytracingGeometryDesc& Desc);

RaytracingScene_t CreateRaytracingScene();

RaytracingPipelineState_t CreateRaytracingPipelineState(const RaytracingPipelineStateDesc& Desc);

RaytracingShaderTable_t CreateRaytracingShaderTable(RaytracingPipelineState_t RaytracingPipelineState, const RaytracingShaderTableLayout& Layout);

// Main thread only. Each prepare must be followed by the matching CommandList build in the same frame.
// Preparing an already built geometry moves it to new memory, so scenes that reference it must be rebuilt in the same frame.
bool PrepareRaytracingGeometryBuild(RaytracingGeometry_t Geometry);
bool PrepareRaytracingSceneBuild(RaytracingScene_t Scene, uint32_t InstanceCount);

// Main thread only, after PrepareRaytracingSceneBuild with the same Count. Writes Count * RaytracingInstanceDescSize bytes to Dst,
// which must be 16 byte aligned. Each instance's geometry must have been built, or prepared this frame.
void WriteRaytracingInstances(RaytracingScene_t Scene, void* Dst, const RaytracingInstance* Src, uint32_t Count);

}