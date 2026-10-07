#include "Impl/PipelineStateImpl.h"

#include "Impl/Dx/d3dx12.h"
#include "RenderImpl.h"
#include "SparseArray.h"

#include <dxcapi.h>

struct IDxcBlob;

namespace rl
{

struct
{
	SparseArray<Dx12GraphicsPipelineStateDesc, GraphicsPipelineState_t> GraphicsPipelines;
	SparseArray<ComPtr<ID3D12PipelineState>, ComputePipelineState_t> ComputePipelines;
} g_pipelines;

static D3D12_BLEND GetBlend(BlendType b)
{
	switch (b)
	{
	case BlendType::ZERO:			return D3D12_BLEND_ZERO;
	case BlendType::ONE:			return D3D12_BLEND_ONE;
	case BlendType::SRC_COLOR:		return D3D12_BLEND_SRC_COLOR;
	case BlendType::INV_SRC_COLOR:	return D3D12_BLEND_INV_SRC_COLOR;
	case BlendType::SRC_ALPHA:		return D3D12_BLEND_SRC_ALPHA;
	case BlendType::INV_SRC_ALPHA:	return D3D12_BLEND_INV_SRC_ALPHA;
	case BlendType::DST_ALPHA:		return D3D12_BLEND_DEST_ALPHA;
	case BlendType::INV_DST_ALPHA:	return D3D12_BLEND_INV_DEST_ALPHA;
	case BlendType::DST_COLOR:		return D3D12_BLEND_DEST_COLOR;
	case BlendType::INV_DST_COLOR:	return D3D12_BLEND_INV_DEST_COLOR;
	case BlendType::SRC_ALPHA_SAT:	return D3D12_BLEND_SRC_ALPHA_SAT;
	}

	assert(0 && "Unsupported blend func");
	return (D3D12_BLEND)0;
}

static D3D12_BLEND_OP GetBlendOp(BlendOp o)
{
	switch (o)
	{
	case BlendOp::ADD:			return D3D12_BLEND_OP_ADD;
	case BlendOp::SUBTRACT:		return D3D12_BLEND_OP_SUBTRACT;
	case BlendOp::REV_SUBTRACT:	return D3D12_BLEND_OP_REV_SUBTRACT;
	case BlendOp::MIN:			return D3D12_BLEND_OP_MIN;
	case BlendOp::MAX:			return D3D12_BLEND_OP_MAX;
	}

	assert(0 && "Unsupported blend op");
	return (D3D12_BLEND_OP)0;
}

static D3D12_FILL_MODE GetFillMode(FillMode fm)
{
	switch (fm)
	{
	case FillMode::SOLID:
		return D3D12_FILL_MODE_SOLID;
	case FillMode::WIREFRAME:
		return D3D12_FILL_MODE_WIREFRAME;
	};

	assert(0 && "Unsupported fill mode");
	return (D3D12_FILL_MODE)0;
}

static D3D12_CULL_MODE GetCullMode(CullMode cm)
{
	switch (cm)
	{
	case CullMode::NONE:
		return D3D12_CULL_MODE_NONE;
	case CullMode::FRONT:
		return D3D12_CULL_MODE_FRONT;
	case CullMode::BACK:
		return D3D12_CULL_MODE_BACK;
	};

	assert(0 && "Unsupported cull mode");
	return (D3D12_CULL_MODE)0;
}

static D3D12_COMPARISON_FUNC GetComparisonFunc(ComparisionFunc f)
{
	switch (f)
	{
	case ComparisionFunc::NEVER:		return D3D12_COMPARISON_FUNC_NEVER;
	case ComparisionFunc::LESS:			return D3D12_COMPARISON_FUNC_LESS;
	case ComparisionFunc::EQUAL:		return D3D12_COMPARISON_FUNC_EQUAL;
	case ComparisionFunc::LESS_EQUAL:	return D3D12_COMPARISON_FUNC_LESS_EQUAL;
	case ComparisionFunc::GREATER:		return D3D12_COMPARISON_FUNC_GREATER;
	case ComparisionFunc::NOT_EQUAL:		return D3D12_COMPARISON_FUNC_NOT_EQUAL;
	case ComparisionFunc::GREATER_EQUAL:	return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
	case ComparisionFunc::ALWAYS:		return D3D12_COMPARISON_FUNC_ALWAYS;
	}

	assert(0 && "Unsupported comparison func");
	return (D3D12_COMPARISON_FUNC)0;
}

static D3D12_INPUT_CLASSIFICATION GetInputClassification(InputClassification ic)
{
	switch (ic)
	{
	case InputClassification::PER_VERTEX: return D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
	case InputClassification::PER_INSTANCE: return D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
	}

	assert(0 && "Unsupported input classification");
	return (D3D12_INPUT_CLASSIFICATION)0;
}

D3D12_PRIMITIVE_TOPOLOGY_TYPE GetPrimTopoType(PrimitiveTopologyType pt)
{
	switch (pt)
	{
	case PrimitiveTopologyType::POINT:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
	case PrimitiveTopologyType::LINE:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
	case PrimitiveTopologyType::TRIANGLE:
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	}

	assert(0 && "Unsupported primitive topology type");
	return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
}

bool CompileGraphicsPipelineState(GraphicsPipelineState_t handle, const GraphicsPipelineStateDesc& desc, const InputElementDesc* inputs, size_t inputCount)
{
	struct Dx12PipelineStateStream
	{
		Dx12PipelineStateStream() = default;
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE RootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_GS GS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_MS MS;
		CD3DX12_PIPELINE_STATE_STREAM_AS AS;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC BlendState;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterizerState;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 DepthStencilState;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_IB_STRIP_CUT_VALUE IBStripCutValue;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RenderTargetFormats;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DepthStencilFormat;

	} StateStream;
	
	{
		RootSignature_t rootSigToUse = desc.RootSignatureOverride;

		if (rootSigToUse == RootSignature_t::INVALID)
			rootSigToUse = g_render.RootSignature;

		StateStream.RootSignature = Dx12_GetRootSignature(rootSigToUse);
	}

	if(desc.VS == VertexShader_t::INVALID && desc.MS == MeshShader_t::INVALID)
	{
		assert(0 && "CompileGraphicsPipelineState needs a valid vertex or mesh shader");
		return false;
	}

	if (desc.VS != VertexShader_t::INVALID && desc.MS != MeshShader_t::INVALID)
	{
		assert(0 && "CompileGraphicsPipelineState needs either a valid vertex or a mesh shader, not both");
		return false;
	}

	if(desc.VS != VertexShader_t::INVALID)
	{
		ComPtr<IDxcBlob> vsBlob = Dx12_GetVertexShaderBlob(desc.VS);
		assert(vsBlob && "CompileGraphicsPipelineState null vsBlob");

		StateStream.VS = { vsBlob->GetBufferPointer(),  vsBlob->GetBufferSize() };
	}
	else if (desc.MS != MeshShader_t::INVALID)
	{
		ComPtr<IDxcBlob> msBlob = Dx12_GetMeshShaderBlob(desc.MS);
		assert(msBlob && "CompileGraphicsPipelineState null msBlob");
		StateStream.MS = { msBlob->GetBufferPointer(),  msBlob->GetBufferSize() };
	}

	if (desc.AS != AmplificationShader_t::INVALID)
	{
		assert(desc.MS != MeshShader_t::INVALID && "Amp shader requires a valid mesh shader");
		ComPtr<IDxcBlob> asBlob = Dx12_GetAmplificationShaderBlob(desc.AS);
		assert(asBlob && "CompileGraphicsPipelineState null asBlob");
		StateStream.AS = { asBlob->GetBufferPointer(),  asBlob->GetBufferSize() };
	}

	if (desc.PS != PixelShader_t::INVALID)
	{
		ComPtr<IDxcBlob> psBlob = Dx12_GetPixelShaderBlob(desc.PS);
		assert(psBlob && "CompileGraphicsPipelineState null psBlob");
		StateStream.PS = { psBlob->GetBufferPointer(),  psBlob->GetBufferSize() };
	}

	if (desc.GS != GeometryShader_t::INVALID)
	{
		ComPtr<IDxcBlob> gsBlob = Dx12_GetGeometryShaderBlob(desc.GS);
		assert(gsBlob && "CompileGraphicsPipelineState null gsBlob");
		StateStream.GS = { gsBlob->GetBufferPointer(),  gsBlob->GetBufferSize() };
	}

	{
		CD3DX12_BLEND_DESC& dxBlendDesc = StateStream.BlendState;

		dxBlendDesc.AlphaToCoverageEnable = FALSE;

		for (uint8_t i = 1; i < desc.TargetDesc.NumRenderTargets; i++)
		{
			if (desc.TargetDesc.Blends[i].Opaque != desc.TargetDesc.Blends[0].Opaque)
			{
				dxBlendDesc.IndependentBlendEnable = true;
				break;
			}
		}

		for (uint8_t i = 0; i < desc.TargetDesc.NumRenderTargets; i++)
		{
			dxBlendDesc.RenderTarget[i].BlendEnable = desc.TargetDesc.Blends[i].BlendEnabled;
			dxBlendDesc.RenderTarget[i].SrcBlend = GetBlend(desc.TargetDesc.Blends[i].SrcBlend);
			dxBlendDesc.RenderTarget[i].DestBlend = GetBlend(desc.TargetDesc.Blends[i].DstBlend);
			dxBlendDesc.RenderTarget[i].BlendOp = GetBlendOp(desc.TargetDesc.Blends[i].Op);
			dxBlendDesc.RenderTarget[i].SrcBlendAlpha = GetBlend(desc.TargetDesc.Blends[i].SrcBlendAlpha);
			dxBlendDesc.RenderTarget[i].DestBlendAlpha = GetBlend(desc.TargetDesc.Blends[i].DstBlendAlpha);
			dxBlendDesc.RenderTarget[i].BlendOpAlpha = GetBlendOp(desc.TargetDesc.Blends[i].OpAlpha);
			dxBlendDesc.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		}
	}

	{
		CD3DX12_RASTERIZER_DESC& dxRasterizerDesc = StateStream.RasterizerState;

		dxRasterizerDesc.FillMode = GetFillMode(desc.Fill);
		dxRasterizerDesc.CullMode = GetCullMode(desc.Cull);
		dxRasterizerDesc.FrontCounterClockwise = FALSE;
		dxRasterizerDesc.DepthBias = desc.DepthBias;
		dxRasterizerDesc.DepthBiasClamp = desc.DepthBiasClamp;
		dxRasterizerDesc.SlopeScaledDepthBias = desc.SlopeScaleDepthBias;
		dxRasterizerDesc.DepthClipEnable = TRUE;
		dxRasterizerDesc.MultisampleEnable = FALSE;
		dxRasterizerDesc.AntialiasedLineEnable = FALSE;
		dxRasterizerDesc.ForcedSampleCount = 0;
		dxRasterizerDesc.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
	}

	{
		CD3DX12_DEPTH_STENCIL_DESC1& dxDepthStencilDesc = StateStream.DepthStencilState;

		dxDepthStencilDesc.DepthEnable = desc.DepthEnabled;
		dxDepthStencilDesc.DepthWriteMask = desc.DepthWrite == DepthWriteMask::ALL ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		dxDepthStencilDesc.DepthFunc = GetComparisonFunc(desc.DepthCompare);
		dxDepthStencilDesc.StencilEnable = FALSE;
		dxDepthStencilDesc.StencilReadMask = 0;
		dxDepthStencilDesc.StencilWriteMask = 0;
	}

	std::vector<D3D12_INPUT_ELEMENT_DESC> dxElements;
	{
		D3D12_INPUT_LAYOUT_DESC& dxLayout = StateStream.InputLayout;		
		dxElements.resize(inputCount);

		for (size_t i = 0; i < inputCount; i++)
		{
			dxElements[i].SemanticName = (LPCSTR)inputs[i].semanticName;
			dxElements[i].SemanticIndex = (UINT)inputs[i].semanticIndex;
			dxElements[i].Format = Dx12_Format(inputs[i].format);
			dxElements[i].InputSlot = (UINT)inputs[i].inputSlot;
			dxElements[i].AlignedByteOffset = (UINT)inputs[i].alignedByteOffset;
			dxElements[i].InputSlotClass = GetInputClassification(inputs[i].inputSlotClass);
			dxElements[i].InstanceDataStepRate = (UINT)inputs[i].instanceDataStepRate;
		}

		dxLayout.NumElements = (UINT)inputCount;
		dxLayout.pInputElementDescs = dxElements.data();
	}

	{
		StateStream.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	}

	{
		StateStream.PrimitiveTopologyType = GetPrimTopoType(desc.PrimTopo);
	}

	{
		D3D12_RT_FORMAT_ARRAY& dxTargets = StateStream.RenderTargetFormats;

		dxTargets.NumRenderTargets = desc.TargetDesc.NumRenderTargets;

		for (uint32_t i = 0; i < desc.TargetDesc.NumRenderTargets; i++)
		{
			dxTargets.RTFormats[i] = Dx12_Format(desc.TargetDesc.Formats[i]);
		}
	}

	{
		StateStream.DepthStencilFormat = Dx12_Format(desc.TargetDesc.DepthFormat);
	}

	Dx12GraphicsPipelineStateDesc& dxPso = g_pipelines.GraphicsPipelines.Alloc(handle);

	D3D12_PIPELINE_STATE_STREAM_DESC dxStreamDesc = {};
	dxStreamDesc.pPipelineStateSubobjectStream = &StateStream;
	dxStreamDesc.SizeInBytes = sizeof(StateStream);

	if (DXENSURE(g_render.DxDevice->CreatePipelineState(&dxStreamDesc, IID_PPV_ARGS(&dxPso.PSO))))
	{
		if(!desc.DebugName.empty())
			dxPso.PSO->SetName(desc.DebugName.c_str());

		dxPso.PrimTopo = Dx12_PrimitiveTopology(desc.PrimTopo);

		return true;
	}	

	return false;
}

bool CompileComputePipelineState(ComputePipelineState_t handle, const ComputePipelineStateDesc& desc)
{
	if (desc.Cs == ComputeShader_t::INVALID)
	{
		assert(0 && "CompileComputePipelineState needs a valid compute shader");
		return false;
	}

	D3D12_COMPUTE_PIPELINE_STATE_DESC dxDesc = {};
	
	RootSignature_t rootSigToUse = desc.RootSignatureOverride;

	if (rootSigToUse == RootSignature_t::INVALID)
		rootSigToUse = g_render.RootSignature;

	dxDesc.pRootSignature = Dx12_GetRootSignature(rootSigToUse);

	ComPtr<IDxcBlob> csBlob = Dx12_GetComputeShaderBlob(desc.Cs);
	assert(csBlob && "CompileComputePipelineState null csBlob");

	dxDesc.CS.pShaderBytecode = csBlob->GetBufferPointer();
	dxDesc.CS.BytecodeLength = csBlob->GetBufferSize();

	dxDesc.NodeMask = 0;

	ComPtr<ID3D12PipelineState>& dxPso = g_pipelines.ComputePipelines.Alloc(handle);

	if (DXENSURE(g_render.DxDevice->CreateComputePipelineState(&dxDesc, IID_PPV_ARGS(&dxPso))))
	{
		if (!desc.DebugName.empty())
			dxPso->SetName(desc.DebugName.c_str());

		return true;
	}		

	return false;
}

void DestroyGraphicsPipelineState(GraphicsPipelineState_t pso)
{
	g_pipelines.GraphicsPipelines.Free(pso);
}

void DestroyComputePipelineState(ComputePipelineState_t pso)
{
	g_pipelines.ComputePipelines.Free(pso);
}

Dx12GraphicsPipelineStateDesc* Dx12_GetPipelineState(GraphicsPipelineState_t pso)
{
	if (g_pipelines.GraphicsPipelines.Valid(pso))
	{
		return &g_pipelines.GraphicsPipelines[pso];
	}

	return nullptr;
}

ID3D12PipelineState* Dx12_GetPipelineState(ComputePipelineState_t pso)
{
	if (g_pipelines.ComputePipelines.Valid(pso))
	{
		return g_pipelines.ComputePipelines[pso].Get();
	}

	return nullptr;
}

}