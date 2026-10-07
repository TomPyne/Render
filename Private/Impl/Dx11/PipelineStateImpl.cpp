#include "Impl/PipelineStateImpl.h"

#include "RenderImpl.h"

namespace rl
{

// TODO: DX12 treats PSOs as unique objects, but in DX11 it is possible to hash and cache each of these state objects
std::vector<Dx11GraphicsPipelineState> g_graphicsPipelines;
std::vector<Dx11ComputePipelineState> g_computePipelines;

static Dx11GraphicsPipelineState* AllocGraphicsPipeline(GraphicsPipelineState_t pso)
{
	if ((size_t)pso >= g_graphicsPipelines.size())
		g_graphicsPipelines.resize((size_t)pso + 1);

	return &g_graphicsPipelines[(size_t)pso];
}

static Dx11ComputePipelineState* AllocComputePipeline(ComputePipelineState_t pso)
{
	if ((size_t)pso >= g_computePipelines.size())
		g_computePipelines.resize((size_t)pso + 1);

	return &g_computePipelines[(size_t)pso];
}

static D3D11_COMPARISON_FUNC GetComparisonFunc(ComparisionFunc f)
{
	switch (f)
	{
	case ComparisionFunc::NEVER:			return D3D11_COMPARISON_NEVER;
	case ComparisionFunc::LESS:				return D3D11_COMPARISON_LESS;
	case ComparisionFunc::EQUAL:			return D3D11_COMPARISON_EQUAL;
	case ComparisionFunc::LESS_EQUAL:		return D3D11_COMPARISON_LESS_EQUAL;
	case ComparisionFunc::GREATER:			return D3D11_COMPARISON_GREATER;
	case ComparisionFunc::NOT_EQUAL:		return D3D11_COMPARISON_NOT_EQUAL;
	case ComparisionFunc::GREATER_EQUAL:	return D3D11_COMPARISON_GREATER_EQUAL;
	case ComparisionFunc::ALWAYS:			return D3D11_COMPARISON_ALWAYS;
	}

	assert(0 && "Unsupported comparison func");
	return (D3D11_COMPARISON_FUNC)0;
}

D3D11_DEPTH_STENCIL_DESC CreateDSS(bool depthEnabled, ComparisionFunc func, DepthWriteMask writeMask)
{
	D3D11_DEPTH_STENCIL_DESC dss;
	ZeroMemory(&dss, sizeof(dss));

	dss.DepthEnable = depthEnabled;
	dss.DepthWriteMask = writeMask == DepthWriteMask::ALL ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
	dss.DepthFunc = GetComparisonFunc(func);

	return dss;
}

D3D11_RASTERIZER_DESC CreateRS(FillMode fm, CullMode cm, int bias, float biasClamp, float slopeBias)
{
	D3D11_RASTERIZER_DESC rs;
	ZeroMemory(&rs, sizeof(rs));

	switch (fm)
	{
	case FillMode::SOLID:
		rs.FillMode = D3D11_FILL_SOLID;
		break;
	case FillMode::WIREFRAME:
		rs.FillMode = D3D11_FILL_WIREFRAME;
		break;
	};

	switch (cm)
	{
	case CullMode::NONE:
		rs.CullMode = D3D11_CULL_NONE;
		break;
	case CullMode::FRONT:
		rs.CullMode = D3D11_CULL_FRONT;
		break;
	case CullMode::BACK:
		rs.CullMode = D3D11_CULL_BACK;
		break;
	};

	rs.ScissorEnable = true;
	rs.DepthClipEnable = true;
	rs.DepthBias = bias;
	rs.DepthBiasClamp = biasClamp;
	rs.SlopeScaledDepthBias = slopeBias;
	return rs;
}

static D3D11_BLEND GetBlend(BlendType b)
{
	switch (b)
	{
	case BlendType::ZERO:			return D3D11_BLEND_ZERO;
	case BlendType::ONE:			return D3D11_BLEND_ONE;
	case BlendType::SRC_COLOR:		return D3D11_BLEND_SRC_COLOR;
	case BlendType::INV_SRC_COLOR:	return D3D11_BLEND_INV_SRC_COLOR;
	case BlendType::SRC_ALPHA:		return D3D11_BLEND_SRC_ALPHA;
	case BlendType::INV_SRC_ALPHA:	return D3D11_BLEND_INV_SRC_ALPHA;
	case BlendType::DST_ALPHA:		return D3D11_BLEND_DEST_ALPHA;
	case BlendType::INV_DST_ALPHA:	return D3D11_BLEND_INV_DEST_ALPHA;
	case BlendType::DST_COLOR:		return D3D11_BLEND_DEST_COLOR;
	case BlendType::INV_DST_COLOR:	return D3D11_BLEND_INV_DEST_COLOR;
	case BlendType::SRC_ALPHA_SAT:	return D3D11_BLEND_SRC_ALPHA_SAT;
	}

	assert(0 && "Unsupported blend func");
	return (D3D11_BLEND)0;
}

static D3D11_BLEND_OP GetBlendOp(BlendOp o)
{
	switch (o)
	{
	case BlendOp::ADD:			return D3D11_BLEND_OP_ADD;
	case BlendOp::SUBTRACT:		return D3D11_BLEND_OP_SUBTRACT;
	case BlendOp::REV_SUBTRACT:	return D3D11_BLEND_OP_REV_SUBTRACT;
	case BlendOp::MIN:			return D3D11_BLEND_OP_MIN;
	case BlendOp::MAX:			return D3D11_BLEND_OP_MAX;
	}

	assert(0 && "Unsupported blend op");
	return (D3D11_BLEND_OP)0;
}

D3D11_BLEND_DESC CreateBS(const BlendMode* bm, uint8_t count)
{
	D3D11_BLEND_DESC bs;
	ZeroMemory(&bs, sizeof(bs));

	for (uint8_t i = 1; i < count; i++)
	{
		if (bm[i].Opaque != bm[0].Opaque)
		{
			bs.IndependentBlendEnable = true;
			break;
		}			
	}

	for (uint8_t i = 0; i < count; i++)
	{
		bs.RenderTarget[i].BlendEnable = bm[i].BlendEnabled;
		bs.RenderTarget[i].SrcBlend = GetBlend(bm[i].SrcBlend);
		bs.RenderTarget[i].DestBlend = GetBlend(bm[i].DstBlend);
		bs.RenderTarget[i].BlendOp = GetBlendOp(bm[i].Op);
		bs.RenderTarget[i].SrcBlendAlpha = GetBlend(bm[i].SrcBlendAlpha);
		bs.RenderTarget[i].DestBlendAlpha = GetBlend(bm[i].DstBlendAlpha);
		bs.RenderTarget[i].BlendOpAlpha = GetBlendOp(bm[i].OpAlpha);
		bs.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
	}

	return bs;
}

D3D11_PRIMITIVE_TOPOLOGY GetPrimTopo(PrimitiveTopologyType pt)
{
	switch (pt)
	{
	case PrimitiveTopologyType::POINT:
		return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
	case PrimitiveTopologyType::LINE:
		return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
	case PrimitiveTopologyType::TRIANGLE:
		return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	}

	assert(0 && "Unsupported primitive topology");
	return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
}

bool CompileGraphicsPipelineState(GraphicsPipelineState_t handle, const GraphicsPipelineStateDesc& desc, const InputElementDesc* inputs, size_t inputCount)
{
	Dx11GraphicsPipelineState* pso = AllocGraphicsPipeline(handle);

	{
		pso->vs = desc.VS;
		pso->gs = desc.GS;
		pso->ps = desc.PS;

		pso->pt = GetPrimTopo(desc.PrimTopo);
	}
	
	{
		D3D11_DEPTH_STENCIL_DESC dss = CreateDSS(desc.DepthEnabled, desc.DepthCompare, desc.DepthWrite);
		if (FAILED(g_render.Device->CreateDepthStencilState(&dss, &pso->dss)))
		{
			fprintf(stderr, "CompileGraphicsPipelineState failed to create depth state");
			return false;
		}			
	}

	{
		D3D11_RASTERIZER_DESC rs = CreateRS(desc.Fill, desc.Cull, desc.DepthBias, desc.DepthBiasClamp, desc.SlopeScaleDepthBias);
		if (FAILED(g_render.Device->CreateRasterizerState(&rs, &pso->rs)))
		{
			fprintf(stderr, "CompileGraphicsPipelineState failed to create raster state");
			return false;
		}			
	}

	{
		D3D11_BLEND_DESC bs = CreateBS(desc.TargetDesc.Blends, desc.TargetDesc.NumRenderTargets);
		if (FAILED(g_render.Device->CreateBlendState(&bs, &pso->bs)))
		{
			fprintf(stderr, "CompileGraphicsPipelineState failed to create blend state");
			return false;
		}			
	}

	if (inputs != nullptr && inputCount && desc.VS != VertexShader_t::INVALID)
	{
		ID3DBlob* blob = Dx11_GetVertexShaderBlob(desc.VS);
		assert(blob != nullptr && "Failed creating input layout, vertex blob is null");

		std::vector<D3D11_INPUT_ELEMENT_DESC> dxLayout;
		dxLayout.resize(inputCount);

		for (size_t i = 0; i < inputCount; i++)
		{
			dxLayout[i].AlignedByteOffset = (UINT)inputs[i].alignedByteOffset;
			dxLayout[i].Format = Dx11_Format(inputs[i].format);
			dxLayout[i].InputSlot = (UINT)inputs[i].inputSlot;
			dxLayout[i].InstanceDataStepRate = (UINT)inputs[i].instanceDataStepRate;
			dxLayout[i].SemanticIndex = (UINT)inputs[i].semanticIndex;
			dxLayout[i].SemanticName = (LPCSTR)inputs[i].semanticName;

			switch (inputs[i].inputSlotClass)
			{
			case InputClassification::PER_VERTEX:
				dxLayout[i].InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
				break;
			case InputClassification::PER_INSTANCE:
				dxLayout[i].InputSlotClass = D3D11_INPUT_PER_INSTANCE_DATA;
				break;
			};
		}

		if (FAILED(g_render.Device->CreateInputLayout(dxLayout.data(), (UINT)dxLayout.size(), blob->GetBufferPointer(), blob->GetBufferSize(), &pso->il)))
		{
			fprintf(stderr, "CompileGraphicsPipelineState failed to create input layout");
			return false;
		}				
	}

	return true;
}

bool CompileComputePipelineState(ComputePipelineState_t handle, const ComputePipelineStateDesc& desc)
{
	if (desc.Cs == ComputeShader_t::INVALID)
		return false;

	AllocComputePipeline(handle)->_cs = desc.Cs;

	return true;
}

Dx11GraphicsPipelineState* Dx11_GetGraphicsPipelineState(GraphicsPipelineState_t pso)
{
	return &g_graphicsPipelines[(uint32_t)pso];
}

Dx11ComputePipelineState* Dx11_GetComputePipelineState(ComputePipelineState_t pso)
{
	return &g_computePipelines[(uint32_t)pso];
}

void DestroyGraphicsPipelineState(GraphicsPipelineState_t pso)
{
	g_graphicsPipelines[(uint32_t)pso] = {};
}

void DestroyComputePipelineState(ComputePipelineState_t pso)
{
	g_computePipelines[(uint32_t)pso] = {};
}

}