#pragma once

#include "Shaders.h"

#include "RenderTypes.h"
#include "RootSignature.h"

namespace rl
{

enum class PrimitiveTopologyType : uint8_t
{
	UNDEFINED,
	POINT,
	LINE,
	TRIANGLE,
	PATCH,
};

enum class FillMode : uint8_t
{
	SOLID,
	WIREFRAME,
};

enum class CullMode : uint8_t
{
	NONE,
	FRONT,
	BACK,
};

enum class BlendType : uint8_t
{
	ZERO,
	ONE,
	SRC_COLOR,
	INV_SRC_COLOR,
	SRC_ALPHA,
	INV_SRC_ALPHA,
	DST_ALPHA,
	INV_DST_ALPHA,
	DST_COLOR,
	INV_DST_COLOR,
	SRC_ALPHA_SAT,
	COUNT,
};

enum class BlendOp : uint8_t
{
	ADD,
	SUBTRACT,
	REV_SUBTRACT,
	MIN,
	MAX,
	COUNT,
};

struct BlendMode
{
	union
	{
		struct
		{
			bool BlendEnabled : 1;
			BlendType SrcBlend : 4;
			BlendType DstBlend : 4;
			BlendOp Op : 3;
			BlendType SrcBlendAlpha : 4;
			BlendType DstBlendAlpha : 4;
			BlendOp OpAlpha : 3;
		};
		uint32_t Opaque = 0;
	};

	BlendMode& Enabled(bool e) noexcept { BlendEnabled = e; return *this; }
	BlendMode& Blend(BlendType s, BlendOp o, BlendType d) noexcept { SrcBlend = s; Op = o; DstBlend = d; return *this; }
	BlendMode& BlendAlpha(BlendType s, BlendOp o, BlendType d) noexcept { SrcBlendAlpha = s; OpAlpha = o; DstBlendAlpha = d; return *this; }

	constexpr BlendMode()
		: BlendEnabled(false)
		, SrcBlend(BlendType::ONE)
		, DstBlend(BlendType::ONE)
		, Op(BlendOp::ADD)
		, SrcBlendAlpha(BlendType::ONE)
		, DstBlendAlpha(BlendType::ONE)
		, OpAlpha(BlendOp::ADD)
	{}

	constexpr BlendMode(bool enabled, BlendType srcBlend, BlendType dstBlend, BlendOp op, BlendType srcBlendAlpha, BlendType dstBlendAlpha, BlendOp opAlpha)
		: BlendEnabled(enabled)
		, SrcBlend(srcBlend)
		, DstBlend(dstBlend)
		, Op(op)
		, SrcBlendAlpha(srcBlendAlpha)
		, DstBlendAlpha(dstBlendAlpha)
		, OpAlpha(opAlpha)
	{}

	static constexpr BlendMode Default() { return BlendMode(true, BlendType::SRC_ALPHA, BlendType::INV_SRC_ALPHA, BlendOp::ADD, BlendType::ONE, BlendType::ONE, BlendOp::ADD); }
	static constexpr BlendMode Add() { return BlendMode(true, BlendType::ONE, BlendType::ONE, BlendOp::ADD, BlendType::ONE, BlendType::ONE, BlendOp::ADD); }
	static constexpr BlendMode None() { return BlendMode(); }
};

enum class ComparisionFunc : uint8_t
{
	NEVER,
	LESS,
	EQUAL,
	LESS_EQUAL,
	GREATER,
	NOT_EQUAL,
	GREATER_EQUAL,
	ALWAYS,
};

struct GraphicsPipelineTargetDesc
{
	static constexpr uint32_t MaxRenderTargets = 8u;

	RenderFormat Formats[MaxRenderTargets] = {};
	BlendMode Blends[MaxRenderTargets] = {};
	uint8_t NumRenderTargets = 0;
	RenderFormat DepthFormat = RenderFormat::UNKNOWN;

	GraphicsPipelineTargetDesc() = default;
	GraphicsPipelineTargetDesc(std::initializer_list<RenderFormat> targetDescs, std::initializer_list<BlendMode> blends, RenderFormat depthFormat);

	uint64_t Hash() const;
private:
	mutable uint64_t Hashed = 0;
};

struct GraphicsPipelineStateDesc
{
	// Rasterizer desc
	PrimitiveTopologyType PrimTopo = PrimitiveTopologyType::UNDEFINED;
	FillMode Fill = FillMode::SOLID;
	CullMode Cull = CullMode::BACK;
	int DepthBias = 0;
	float DepthBiasClamp = 0.0f;
	float SlopeScaleDepthBias = 0.0f;

	// Depth desc
	bool DepthEnabled = false;
	ComparisionFunc DepthCompare = ComparisionFunc::NEVER;
	
	GraphicsPipelineTargetDesc TargetDesc = {};
	
	VertexShader_t VS = VertexShader_t::INVALID;
	GeometryShader_t GS = GeometryShader_t::INVALID;
	MeshShader_t MS = MeshShader_t::INVALID;
	AmplificationShader_t AS = AmplificationShader_t::INVALID;
	PixelShader_t PS = PixelShader_t::INVALID;

	RootSignature_t RootSignatureOverride = RootSignature_t::INVALID;

	std::wstring DebugName;

	GraphicsPipelineStateDesc& RasterizerDesc(PrimitiveTopologyType pt, FillMode fm, CullMode cm, int bias = 0, float biasClamp = 0.0f, float slopeScaleBias = 0.0f)
	{ 
		PrimTopo = pt; 
		Fill = fm; 
		Cull = cm; 
		DepthBias = bias;
		DepthBiasClamp = biasClamp;
		SlopeScaleDepthBias = slopeScaleBias;
		return *this; 
	}

	GraphicsPipelineStateDesc& DepthDesc(bool enabled, ComparisionFunc cf = ComparisionFunc::NEVER) 
	{
		DepthEnabled = enabled; 
		DepthCompare = cf;
		return *this; 
	}

	GraphicsPipelineStateDesc& TargetBlendDesc(std::initializer_list<RenderFormat> targetDescs, std::initializer_list<BlendMode> blends, RenderFormat depthFormat)
	{
		TargetDesc = GraphicsPipelineTargetDesc(targetDescs, blends, depthFormat);

		return *this;
	}

	GraphicsPipelineStateDesc& TargetBlendDesc(const GraphicsPipelineTargetDesc& desc)
	{
		TargetDesc = desc;

		return *this;
	}

	GraphicsPipelineStateDesc& VertexShader(VertexShader_t vs) { VS = vs; return *this; }
	GraphicsPipelineStateDesc& GeometryShader(GeometryShader_t gs) { GS = gs; return *this; }
	GraphicsPipelineStateDesc& MeshShader(MeshShader_t ms) { MS = ms; return *this; }
	GraphicsPipelineStateDesc& AmplificationShader(AmplificationShader_t as) { AS = as; return *this; }
	GraphicsPipelineStateDesc& PixelShader(PixelShader_t ps) { PS = ps; return *this; }
	GraphicsPipelineStateDesc& RootSignature(RootSignature_t rs) { RootSignatureOverride = rs; return *this; }
};

struct ComputePipelineStateDesc
{
	ComputeShader_t Cs = ComputeShader_t::INVALID;
	RootSignature_t RootSignatureOverride = RootSignature_t::INVALID;

	std::wstring DebugName;
};

GraphicsPipelineState_t CreateGraphicsPipelineState(const GraphicsPipelineStateDesc& desc, const InputElementDesc* inputs = nullptr, size_t inputCount = 0);
ComputePipelineState_t CreateComputePipelineState(const ComputePipelineStateDesc& desc);

size_t GetGraphicsPipelineStateCount();
size_t GetComputePipelineStateCount();

void ReloadPipelines();

}