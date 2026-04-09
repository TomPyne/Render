#include "CommandList.h"

#include "IndirectCommands.h"
#include "RenderImpl.h"

namespace rl
{

std::vector<CommandListPtr> g_FreeCommandLists;

struct CommandListImpl
{
	ComPtr<ID3D11DeviceContext> Context = nullptr;
	ComPtr<ID3D11CommandList> CommandList = nullptr;

	RootSignature_t BoundRootSignature = RootSignature_t::INVALID;
	GraphicsPipelineState_t LastPipeline = GraphicsPipelineState_t::INVALID;
	ComputePipelineState_t LastComputePipeline = ComputePipelineState_t::INVALID;
};

CommandList::CommandList(CommandListImpl* cl)
{
	Impl = std::unique_ptr<CommandListImpl>(cl);
}

CommandList::~CommandList()
{
}

void CommandList::Begin()
{
	Impl->Context->ClearState();
}

void CommandList::Finish()
{
	Impl->Context->FinishCommandList(FALSE, &Impl->CommandList);
	Impl->LastPipeline = GraphicsPipelineState_t::INVALID;
}

void CommandList::SetRootSignature()
{
	SetRootSignature(g_render.MainRootSig);
}

void CommandList::SetRootSignature(RootSignature_t rs)
{
	if (rs != Impl->BoundRootSignature)
	{
		const RootSignatureSamplers* globalSamplers = Dx11_GetGlobalSamplers(rs);

		if (globalSamplers)
		{
			if (!globalSamplers->AllSamplers.DxSamplersRaw.empty())
			{
				Impl->Context->VSSetSamplers(0, (UINT)globalSamplers->AllSamplers.DxSamplersRaw.size(), globalSamplers->AllSamplers.DxSamplersRaw.data());
				Impl->Context->CSSetSamplers(0, (UINT)globalSamplers->AllSamplers.DxSamplersRaw.size(), globalSamplers->AllSamplers.DxSamplersRaw.data());
				Impl->Context->GSSetSamplers(0, (UINT)globalSamplers->AllSamplers.DxSamplersRaw.size(), globalSamplers->AllSamplers.DxSamplersRaw.data());
				Impl->Context->PSSetSamplers(0, (UINT)globalSamplers->AllSamplers.DxSamplersRaw.size(), globalSamplers->AllSamplers.DxSamplersRaw.data());
			}

			if (!globalSamplers->VertexSamplers.DxSamplers.empty())
			{
				Impl->Context->VSSetSamplers(0, (UINT)globalSamplers->VertexSamplers.DxSamplersRaw.size(), globalSamplers->VertexSamplers.DxSamplersRaw.data());
			}

			if (!globalSamplers->GeometrySamplers.DxSamplers.empty())
			{
				Impl->Context->VSSetSamplers(0, (UINT)globalSamplers->GeometrySamplers.DxSamplersRaw.size(), globalSamplers->GeometrySamplers.DxSamplersRaw.data());
			}

			if (!globalSamplers->PixelSamplers.DxSamplers.empty())
			{
				Impl->Context->VSSetSamplers(0, (UINT)globalSamplers->PixelSamplers.DxSamplersRaw.size(), globalSamplers->PixelSamplers.DxSamplersRaw.data());
			}
		}

		Impl->BoundRootSignature = rs;
	}
}

void CommandList::ClearRenderTarget(RenderTargetView_t rtv, const float col[4])
{
	ID3D11RenderTargetView* dxRtv = Dx11_GetRenderTargetView(rtv);
	Impl->Context->ClearRenderTargetView(dxRtv, col);
}

void CommandList::ClearDepth(DepthStencilView_t dsv, float depth)
{
	ID3D11DepthStencilView* dxDsv = Dx11_GetDepthStencilView(dsv);
	Impl->Context->ClearDepthStencilView(dxDsv, D3D11_CLEAR_DEPTH, depth, 0);
}

void CommandList::SetRenderTargets(const RenderTargetView_t* const rtvs, size_t num, DepthStencilView_t dsv)
{
	assert(num <= 8);

	ID3D11RenderTargetView* dxRtvs[8];

	for(size_t i = 0; i < num; i++)
		dxRtvs[i] = Dx11_GetRenderTargetView(rtvs[i]);

	ID3D11DepthStencilView* dxDsv = Dx11_GetDepthStencilView(dsv);

	Impl->Context->OMSetRenderTargets((UINT)num, dxRtvs, dxDsv);
}

void CommandList::SetViewports(const Viewport* const vps, size_t num)
{
	assert(num <= 8);

	D3D11_VIEWPORT dxVps[8];
	for (size_t i = 0; i < num; i++)
	{
		dxVps[i].TopLeftX	= vps[i].topLeftX;
		dxVps[i].TopLeftY	= vps[i].topLeftY;
		dxVps[i].Width		= vps[i].width;
		dxVps[i].Height		= vps[i].height;
		dxVps[i].MinDepth	= vps[i].minDepth;
		dxVps[i].MaxDepth	= vps[i].maxDepth;
	}

	Impl->Context->RSSetViewports((UINT)num, dxVps);
}

void CommandList::SetDefaultScissor()
{
	D3D11_RECT rect = CD3D11_RECT(0, 0, LONG_MAX, LONG_MAX);
	Impl->Context->RSSetScissorRects(1, &rect);
}

void CommandList::SetScissors(const ScissorRect* const scissors, size_t num)
{
	assert(num <= 8);
	D3D11_RECT dxRects[8];
	for (size_t i = 0; i < num; i++)
	{
		dxRects[i].left = scissors[i].left;
		dxRects[i].top = scissors[i].top;
		dxRects[i].right = scissors[i].right;
		dxRects[i].bottom = scissors[i].bottom;
	}

	Impl->Context->RSSetScissorRects((UINT)num, dxRects);
}

void CommandList::SetPipelineState(GraphicsPipelineState_t pso)
{
	if (pso == Impl->LastPipeline)
		return;

	Impl->LastComputePipeline = ComputePipelineState_t::INVALID;
	Impl->LastPipeline = pso;

	Dx11GraphicsPipelineState* dxPso = Dx11_GetGraphicsPipelineState(pso);

	if (!dxPso)
	{
		return;
	}	

	Impl->Context->IASetPrimitiveTopology(dxPso->pt);
	Impl->Context->IASetInputLayout(dxPso->il.Get());
	Impl->Context->OMSetDepthStencilState(dxPso->dss.Get(), 0);
	Impl->Context->RSSetState(dxPso->rs.Get());
	Impl->Context->OMSetBlendState(dxPso->bs.Get(), nullptr, 0xffffffff);
	Impl->Context->VSSetShader(Dx11_GetVertexShader(dxPso->vs), nullptr, 0);
	Impl->Context->GSSetShader(Dx11_GetGeometryShader(dxPso->gs), nullptr, 0);
	Impl->Context->PSSetShader(Dx11_GetPixelShader(dxPso->ps), nullptr, 0);
	Impl->Context->CSSetShader(nullptr, nullptr, 0);
}

void CommandList::SetPipelineState(ComputePipelineState_t pso)
{
	if (pso == Impl->LastComputePipeline)
		return;

	Impl->LastPipeline = GraphicsPipelineState_t::INVALID;
	Impl->LastComputePipeline = pso;

	Dx11ComputePipelineState* dxPso = Dx11_GetComputePipelineState(pso);

	if(!dxPso)
	{
		return;
	}

	Impl->Context->CSSetShader(Dx11_GetComputeShader(dxPso->_cs), nullptr, 0);
}

void CommandList::SetPipelineState(RaytracingPipelineState_t pso) { assert(0); }

void CommandList::SetVertexBuffers(uint32_t startSlot, uint32_t count, const VertexBuffer_t* const vbs, const uint32_t* const strides, const uint32_t* const offsets)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; slot++, i++)
	{
		SetVertexBuffer(slot, vbs[i], strides[i], offsets[i]);
	}
}

void CommandList::SetVertexBuffers(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const vbs, const uint32_t* const strides, const uint32_t* const offsets)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; slot++, i++)
	{
		SetVertexBuffer(slot, vbs[i], strides[i], offsets[i]);
	}
}

void CommandList::SetVertexBuffer(uint32_t slot, VertexBuffer_t vb, uint32_t stride, uint32_t offset)
{
	ID3D11Buffer* dxVb = Dx11_GetVertexBuffer(vb);
	Impl->Context->IASetVertexBuffers(slot, 1, &dxVb, (const UINT*)(&stride), (const UINT*)(&offset));
}

void CommandList::SetVertexBuffer(uint32_t slot, DynamicBuffer_t vb, uint32_t stride, uint32_t offset)
{
	ID3D11Buffer* dxVb = Dx11_GetDynamicBuffer(vb);
	Impl->Context->IASetVertexBuffers(slot, 1, &dxVb, (const UINT*)(&stride), (const UINT*)(&offset));
}

void CommandList::SetIndexBuffer(IndexBuffer_t ib, RenderFormat format, uint32_t indexOffset)
{
	ID3D11Buffer* dxIb = Dx11_GetIndexBuffer(ib);
	Impl->Context->IASetIndexBuffer(dxIb, Dx11_Format(format), (UINT)indexOffset);
}

void CommandList::SetIndexBuffer(DynamicBuffer_t ib, RenderFormat format, uint32_t indexOffset)
{
	ID3D11Buffer* dxIb = Dx11_GetDynamicBuffer(ib);
	Impl->Context->IASetIndexBuffer(dxIb, Dx11_Format(format), (UINT)indexOffset);
}

void CommandList::CopyTexture(Texture_t dst, Texture_t src)
{
	ID3D11Resource* dxDst = Dx11_GetTexture(dst);
	ID3D11Resource* dxSrc = Dx11_GetTexture(src);

	Impl->Context->CopyResource(dxDst, dxSrc);
}

void CommandList::DrawIndexedInstanced(uint32_t numIndices, uint32_t numInstances, uint32_t startIndex, uint32_t startVertex, uint32_t startInstance)
{
	Impl->Context->DrawIndexedInstanced((UINT)numIndices, (UINT)numInstances, (UINT)startIndex, (UINT)startVertex, (UINT)startInstance);
}

void CommandList::DrawInstanced(uint32_t numVerts, uint32_t numInstances, uint32_t startVertex, uint32_t startInstance)
{
	Impl->Context->DrawInstanced((UINT)numVerts, (UINT)numInstances, (UINT)startVertex, (UINT)startInstance);
}

void CommandList::ExecuteIndirect(IndirectCommand_t ic, StructuredBuffer_t argBuf, uint64_t argBufferOffset)
{
	IndirectCommandType commandType = GetIndirectCommandType(ic);
	ID3D11Buffer* dxRes = Dx11_GetStructuredBuffer(argBuf);
	switch (commandType)
	{
	case IndirectCommandType::INDIRECT_DRAW:
		Impl->Context->DrawInstancedIndirect(dxRes, (UINT)argBufferOffset);
		break;
	case IndirectCommandType::INDIRECT_DRAW_INDEXED:
		Impl->Context->DrawIndexedInstancedIndirect(dxRes, (UINT)argBufferOffset);
		break;
	case IndirectCommandType::INDIRECT_DISPATCH:
		Impl->Context->DispatchIndirect(dxRes, (UINT)argBufferOffset);
		break;
	}
}

void CommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
	Impl->Context->Dispatch(x, y, z);
}

void CommandList::DispatchMesh(uint32_t x, uint32_t y, uint32_t z) { assert(0); }

void CommandList::BuildRaytracingScene(RaytracingScene_t scene) { assert(0); }

void CommandList::DispatchRays(RaytracingShaderTable_t ShaderTable, uint32_t X, uint32_t Y, uint32_t Z) { assert(0); }

// Dx11 Style Bind Commands
void CommandList::BindVertexSRVs(uint32_t startSlot, uint32_t count, const ShaderResourceView_t* const srvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11ShaderResourceView* dxSrv = Dx11_GetShaderResourceView(srvs[i]);
		Impl->Context->VSSetShaderResources(slot, 1, &dxSrv);
	}
}

void CommandList::BindVertexCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11Buffer* dxCbv = Dx11_GetConstantBuffer(cbvs[i]);
		Impl->Context->VSSetConstantBuffers(slot, 1, &dxCbv);
	}
}

void CommandList::BindVertexCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11Buffer* dxCbv = Dx11_GetDynamicBuffer(cbvs[i]);
		Impl->Context->VSSetConstantBuffers(slot, 1, &dxCbv);
	}
}

void CommandList::BindGeometryCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11Buffer* dxCbv = Dx11_GetConstantBuffer(cbvs[i]);
		Impl->Context->GSSetConstantBuffers(slot, 1, &dxCbv);
	}
}

void CommandList::BindGeometryCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11Buffer* dxCbv = Dx11_GetDynamicBuffer(cbvs[i]);
		Impl->Context->GSSetConstantBuffers(slot, 1, &dxCbv);
	}
}

void CommandList::BindPixelSRVs(uint32_t startSlot, uint32_t count, const ShaderResourceView_t* const srvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11ShaderResourceView* dxSrv = Dx11_GetShaderResourceView(srvs[i]);
		Impl->Context->PSSetShaderResources(slot, 1, &dxSrv);
	}
}

void CommandList::BindPixelCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11Buffer* dxCbv = Dx11_GetConstantBuffer(cbvs[i]);
		Impl->Context->PSSetConstantBuffers(slot, 1, &dxCbv);
	}
}

void CommandList::BindPixelCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11Buffer* dxCbv = Dx11_GetDynamicBuffer(cbvs[i]);
		Impl->Context->PSSetConstantBuffers(slot, 1, &dxCbv);
	}
}

void CommandList::BindComputeSRVs(uint32_t startSlot, uint32_t count, const ShaderResourceView_t* const srvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11ShaderResourceView* dxSrv = Dx11_GetShaderResourceView(srvs[i]);
		Impl->Context->CSSetShaderResources(slot, 1, &dxSrv);
	}
}

void CommandList::BindComputeUAVs(uint32_t startSlot, uint32_t count, const UnorderedAccessView_t* const srvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11UnorderedAccessView* dxUav = Dx11_GetUnorderedAccessView(srvs[i]);
		Impl->Context->CSSetUnorderedAccessViews(slot, 1, &dxUav, nullptr);
	}
}

void CommandList::BindComputeCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11Buffer* dxCbv = Dx11_GetConstantBuffer(cbvs[i]);
		Impl->Context->CSSetConstantBuffers(slot, 1, &dxCbv);
	}
}

void CommandList::BindComputeCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs)
{
	const UINT endSlot = startSlot + count;
	UINT slot = startSlot;
	for (UINT i = 0; slot < endSlot; i++, slot++)
	{
		ID3D11Buffer* dxCbv = Dx11_GetDynamicBuffer(cbvs[i]);
		Impl->Context->CSSetConstantBuffers(slot, 1, &dxCbv);
	}
}

void CommandList::SetGraphicsRootValue(uint32_t slot, uint32_t offset, uint32_t value) { assert(0); }
void CommandList::SetComputeRootValue(uint32_t slot, uint32_t offset, uint32_t value) { assert(0); }
void CommandList::SetGraphicsRootCBV(uint32_t slot, ConstantBuffer_t cb) { assert(0); }
void CommandList::SetComputeRootCBV(uint32_t slot, ConstantBuffer_t cb) { assert(0); }
void CommandList::SetGraphicsRootCBV(uint32_t slot, DynamicBuffer_t cb) { assert(0); }
void CommandList::SetComputeRootCBV(uint32_t slot, DynamicBuffer_t cb) { assert(0); }
void CommandList::SetGraphicsRootSRV(uint32_t slot, ShaderResourceView_t srv) { assert(0); }
void CommandList::SetComputeRootSRV(uint32_t slot, ShaderResourceView_t srv) { assert(0); }
void CommandList::SetGraphicsRootUAV(uint32_t slot, UnorderedAccessView_t uav) { assert(0); }
void CommandList::SetComputeRootUAV(uint32_t slot, UnorderedAccessView_t uav) { assert(0); }
void CommandList::SetGraphicsRootDescriptorTable(uint32_t slot) { assert(0); }
void CommandList::SetComputeRootDescriptorTable(uint32_t slot) { assert(0); }

void CommandList::TransitionResource(Texture_t tex, ResourceTransitionState before, ResourceTransitionState after) {}
void CommandList::TransitionResource(StructuredBuffer_t buf, ResourceTransitionState before, ResourceTransitionState after) {}
void CommandList::UAVBarrier(Texture_t tex) {}
void CommandList::UAVBarrier(StructuredBuffer_t buf) {}

void CommandList::BeginEvent(const char* EventStr) {}

void CommandList::BeginEvent(const wchar_t* EventStr) {}

void CommandList::EndEvent() {}

void CommandList::AddMarker(const char* MarkerStr) {}

void CommandList::AddMarker(const wchar_t* MarkerStr) {}

CommandListPtr CommandList::Create()
{
	if (!g_FreeCommandLists.empty())
	{
		CommandListPtr cl = g_FreeCommandLists.back();
		g_FreeCommandLists.pop_back();

		cl->Begin();

		return cl;
	}
	else
	{
		CommandListImpl* impl = new CommandListImpl;

		g_render.Device->CreateDeferredContext(0, &impl->Context);

		CommandListPtr cl = std::make_shared<CommandList>(impl);

		cl->Begin();

		return cl;
	}
}

CommandListPtr CommandList::Create(CommandListType type)
{
	return CommandList::Create();
}

CommandList* CommandList::CreateRaw(CommandListType type)
{
	CommandListImpl* impl = new CommandListImpl;

	g_render.Device->CreateDeferredContext(0, &impl->Context);

	CommandList* cl = new CommandList(impl);

	cl->Begin();

	return cl;
}

void CommandList::Execute(CommandListPtr& cl)
{
	assert(cl);

	cl->Finish();

	assert(cl->Impl->CommandList);

	g_render.DeviceContext->ExecuteCommandList(cl->Impl->CommandList.Get(), FALSE);

	g_FreeCommandLists.push_back(cl);
}

void CommandList::ExecuteAndStall(CommandListPtr& cl)
{
	D3D11_QUERY_DESC qDesc = {};
	qDesc.Query = D3D11_QUERY_EVENT;

	ComPtr<ID3D11Query> dxQuery;

	g_render.Device->CreateQuery(&qDesc, &dxQuery);

	cl->Finish();

	g_render.DeviceContext->ExecuteCommandList(cl->Impl->CommandList.Get(), FALSE);

	g_render.DeviceContext->End(dxQuery.Get());

	UINT32 queryData;
	while (g_render.DeviceContext->GetData(dxQuery.Get(), &queryData, sizeof(queryData), 0) != S_OK)
		Sleep(5);
}

void CommandList::ReleaseAll()
{
	g_FreeCommandLists.clear();
}

CommandListImpl* CommandList::GetCommandListImpl()
{
	return Impl.get();
}

CommandList* CommandListSubmissionGroup::CreateCommandList()
{
	CommandLists.emplace_back(std::unique_ptr<CommandList>(CommandList::CreateRaw(Type)));

	return CommandLists.back().get();
}

void CommandListSubmissionGroup::Submit()
{
	for (size_t i = 0; i < CommandLists.size(); i++)
	{
		CommandListImpl* impl = CommandLists[i]->GetCommandListImpl();

		impl->Context->FinishCommandList(FALSE, &impl->CommandList);

		g_render.DeviceContext->ExecuteCommandList(impl->CommandList.Get(), FALSE);
		
		//g_FreeCommandLists.push_back(CommandListPtr(CommandLists[i].release()));
	}
}

}