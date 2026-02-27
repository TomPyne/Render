#include "CommandList.h"

#include "Raytracing.h"
#include "RenderImpl.h"
#include "RootSignature.h"
#include <mutex>

namespace rl
{

struct Dx12CommandListPool
{
	std::mutex Mutex;
	std::vector<Dx12CommandAllocator> AvailableAllocators;
	std::vector<ComPtr<ID3D12GraphicsCommandList6>> AvailableCommandLists;
};

struct Dx12CommandLists
{
	Dx12CommandListPool DirectCommandListPool;
	Dx12CommandListPool ComputeCommandListPool;
	Dx12CommandListPool CopyCommandListPool;
	
} g_commandLists;

struct CommandListImpl
{
	Dx12CommandList CL = {};

	ID3D12GraphicsCommandList* cl = nullptr;

	// We dont need to accquire all these heaps for every cl.
	// TODO: consider lazy accquisition
	Dx12DescriptorHeap SrvUavHeap = {};
	Dx12DescriptorHeap RtvHeap = {};
	Dx12DescriptorHeap DsvHeap = {};

	RootSignature_t BoundRootSignature = RootSignature_t::INVALID;

	GraphicsPipelineState_t LastPipeline = GraphicsPipelineState_t::INVALID;
	ComputePipelineState_t LastComputePipeline = ComputePipelineState_t::INVALID;
	RaytracingPipelineState_t LastRTPipeline = RaytracingPipelineState_t::INVALID;

	CommandListImpl(Dx12CommandList&& cl) : CL(cl) {}

	D3D12_CPU_DESCRIPTOR_HANDLE RtvCpuDescriptorHandle(RenderTargetView_t rtv)
	{
		return Dx12_RtvDescriptorHandle(RtvHeap.DxHeap.Get(), rtv);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE DsvCpuDescriptorHandle(DepthStencilView_t dsv)
	{
		return Dx12_DsvDescriptorHandle(DsvHeap.DxHeap.Get(), dsv);
	}

	D3D12_GPU_VIRTUAL_ADDRESS SrvGpuAdress(ShaderResourceView_t srv)
	{
		return Dx12_GetSrvAddress(SrvUavHeap.DxHeap.Get(), srv);
	}

	D3D12_GPU_VIRTUAL_ADDRESS UavGpuAdress(UnorderedAccessView_t uav)
	{
		return Dx12_GetUavAddress(SrvUavHeap.DxHeap.Get(), uav);
	}

	D3D12_GPU_DESCRIPTOR_HANDLE SrvUavTableDescriptorHandle()
	{
		return Dx12_GetSrvUavTableHandle(SrvUavHeap.DxHeap.Get());
	}

	void SubmitHeaps(CommandListType type, uint64_t fenceValue)
	{
		uint64_t graphicsFence = type == CommandListType::GRAPHICS ? fenceValue : 0;
		uint64_t computeFence = type == CommandListType::COMPUTE ? fenceValue : 0;

		if (SrvUavHeap.DxHeap)
		{
			Dx12_SubmitSrvUavDescriptorHeap(std::move(SrvUavHeap), graphicsFence, computeFence);
			SrvUavHeap = {};
		}

		if (RtvHeap.DxHeap)
		{
			Dx12_SubmitRtvDescriptorHeap(std::move(RtvHeap), graphicsFence, computeFence);
			RtvHeap = {};
		}

		if (DsvHeap.DxHeap)
		{
			Dx12_SubmitDsvDescriptorHeap(std::move(DsvHeap), graphicsFence, computeFence);
			DsvHeap = {};
		}
	}
};

static Dx12CommandListPool* GetCommandListPoolForType(D3D12_COMMAND_LIST_TYPE type)
{
	Dx12CommandListPool* pool = nullptr;
	switch (type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT: return &g_commandLists.DirectCommandListPool;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE: return &g_commandLists.ComputeCommandListPool;
	case D3D12_COMMAND_LIST_TYPE_COPY: return &g_commandLists.CopyCommandListPool;
	}

	assert(0 && "GetCommandListPoolForType passed unsupported type");
	return nullptr;
}

static ID3D12Fence* GetCommandQueueFenceForType(D3D12_COMMAND_LIST_TYPE type)
{
	ID3D12CommandQueue* queue = nullptr;
	switch (type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT: return g_render.DirectQueue.DxFence.Get();
	case D3D12_COMMAND_LIST_TYPE_COMPUTE: return g_render.ComputeQueue.DxFence.Get();
	case D3D12_COMMAND_LIST_TYPE_COPY: return g_render.CopyQueue.DxFence.Get();
	}

	assert(0 && "GetCommandQueueFenceForType passed unsupported type");
	return nullptr;
}

static Dx12CommandAllocator AccquireCommandAllocator_AssumeLocked(D3D12_COMMAND_LIST_TYPE type)
{
	Dx12CommandListPool* pool = GetCommandListPoolForType(type);
	if (!pool)
		return {};

	Dx12CommandAllocator allocator = {};

	// If some allocators available check for any not used by the command queue
	if (!pool->AvailableAllocators.empty())
	{
		ID3D12Fence* fence = GetCommandQueueFenceForType(type);
		const UINT64 fenceVal = fence->GetCompletedValue();

		const size_t availableAllocatorCnt = pool->AvailableAllocators.size();

		uint32_t i = 0;
		for (; i < availableAllocatorCnt; i++)
		{
			if (pool->AvailableAllocators[i].FenceValue < fenceVal)
			{
				break;
			}
		}		

		if (i < availableAllocatorCnt)
		{
			allocator.DxAllocator = std::move(pool->AvailableAllocators[i].DxAllocator);

			// If not at the end of the array, swap remove
			if (i < availableAllocatorCnt - 1)
			{
				std::swap(pool->AvailableAllocators[i], pool->AvailableAllocators[availableAllocatorCnt - 1]);
			}

			pool->AvailableAllocators.pop_back();
		}		
	}

	// No available allocators, create a new one
	if (allocator.DxAllocator == nullptr)
	{		
		if (!DXENSURE(g_render.DxDevice->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator.DxAllocator))))
		{
			return {};
		}			
	}

	if (allocator.DxAllocator)
	{
		allocator.DxAllocator->Reset();
	}

	return allocator;
}

Dx12CommandList Dx12_AccquireCommandList(CommandListType type)
{
	const D3D12_COMMAND_LIST_TYPE dxType = Dx12_CommandListType(type);
	return Dx12_AccquireCommandList(dxType);
}

Dx12CommandList Dx12_AccquireCommandList(D3D12_COMMAND_LIST_TYPE type)
{
	Dx12CommandListPool* pool = GetCommandListPoolForType(type);
	if (!pool)
		return {};

	std::lock_guard<std::mutex> lock{ pool->Mutex };

	Dx12CommandList cl;
	cl.DxType = type;
	cl.Allocator = AccquireCommandAllocator_AssumeLocked(type);
	
	if (pool->AvailableCommandLists.empty())
	{
		// OPTIMIZATION: Use CreateCommandList1 to create command list in closed state if required
		if (!DXENSURE(g_render.DxDevice->CreateCommandList(1u, type, cl.Allocator.DxAllocator.Get(), nullptr, IID_PPV_ARGS(&cl.DxCl))))
		{
			return {};
		}
	}
	else
	{
		cl.DxCl = std::move(pool->AvailableCommandLists.back());
		pool->AvailableCommandLists.pop_back();

		cl.DxCl->Reset(cl.Allocator.DxAllocator.Get(), nullptr);
	}

	return cl;
}

void Dx12_FlushQueue(Dx12CommandQueue& queue)
{
	queue.FenceValue++;

	queue.DxFence->Signal(queue.FenceValue);

	if (DXENSURE(queue.DxFence->SetEventOnCompletion(queue.FenceValue, queue.FenceEventHandle)))
	{
		WaitForSingleObject(queue.FenceEventHandle, INFINITE);
	}
}

void Dx12_FlushQueues()
{
	Dx12_FlushQueue(g_render.CopyQueue);
	Dx12_FlushQueue(g_render.DirectQueue);
	Dx12_FlushQueue(g_render.ComputeQueue);
}

static void Dx12_ReleaseCommandList(Dx12CommandList& cl)
{
	Dx12CommandListPool* pool = GetCommandListPoolForType(cl.DxType);
	if (!pool)
		return;

	std::lock_guard<std::mutex> lock{ pool->Mutex };

	pool->AvailableAllocators.emplace_back(std::move(cl.Allocator));
	pool->AvailableCommandLists.emplace_back(std::move(cl.DxCl));
}

CommandList::CommandList(CommandListImpl* cl)
{
	Impl = std::unique_ptr<CommandListImpl>(cl);
	Type = CommandListType::GRAPHICS;
}

CommandList::CommandList(CommandListImpl* cl, CommandListType type)
{
	Impl = std::unique_ptr<CommandListImpl>(cl);
	Type = type;
}

CommandList::~CommandList()
{
	Dx12_ReleaseCommandList(Impl->CL);
}

void CommandList::SetRootSignature()
{
	SetRootSignature(g_render.RootSignature);
}

void CommandList::SetRootSignature(RootSignature_t rs)
{
	if (rs != Impl->BoundRootSignature)
	{
		if (Type == CommandListType::GRAPHICS)
		{
			Impl->CL.DxCl->SetGraphicsRootSignature(Dx12_GetRootSignature(rs));
			Impl->CL.DxCl->SetComputeRootSignature(Dx12_GetRootSignature(rs));
		}
		else if (Type == CommandListType::COMPUTE)
		{
			Impl->CL.DxCl->SetComputeRootSignature(Dx12_GetRootSignature(rs));
		}

		Impl->BoundRootSignature = rs;
	}
}

void CommandList::SetComputeRootSignature(RootSignature_t rs)
{
	if (rs != Impl->BoundRootSignature)
	{
		if (Type == CommandListType::GRAPHICS || Type == CommandListType::COMPUTE)
		{
			Impl->CL.DxCl->SetComputeRootSignature(Dx12_GetRootSignature(rs));
		}

		Impl->BoundRootSignature = rs;
	}
}

void CommandList::Begin()
{
	if (Type != CommandListType::COPY)
	{
		if (Type == CommandListType::GRAPHICS)
		{
			Impl->RtvHeap = Dx12_AccquireRtvHeap();
			Impl->DsvHeap = Dx12_AccquireDsvHeap();
		}

		Impl->SrvUavHeap = Dx12_AccquireSrvUavHeap();

		if (Impl->SrvUavHeap.DxHeap)
		{
			ID3D12DescriptorHeap* heaps[] = { Impl->SrvUavHeap.DxHeap.Get() };
			Impl->CL.DxCl->SetDescriptorHeaps(ARRAYSIZE(heaps), heaps);
		}
	}
}

void CommandList::Finish()
{
	Impl->CL.DxCl->Close();
}

void CommandList::ClearRenderTarget(RenderTargetView_t rtv, const float col[4])
{
	Impl->CL.DxCl->ClearRenderTargetView(Impl->RtvCpuDescriptorHandle(rtv), col, 0u, nullptr);
}

void CommandList::ClearDepth(DepthStencilView_t dsv, float depth)
{
	Impl->CL.DxCl->ClearDepthStencilView(Impl->DsvCpuDescriptorHandle(dsv), D3D12_CLEAR_FLAG_DEPTH, depth, 0u, 0u, nullptr);
}

void CommandList::SetRenderTargets(const RenderTargetView_t* const rtvs, size_t num, DepthStencilView_t dsv)
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[8] = { 0 };
	for (uint32_t i = 0; i < num && i < 8u; i++)
	{
		rtvHandles[i] = Impl->RtvCpuDescriptorHandle(rtvs[i]);
	}

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = {};
	
	if (dsv != DepthStencilView_t::INVALID)
	{
		dsvHandle = Impl->DsvCpuDescriptorHandle(dsv);
	}	

	Impl->CL.DxCl->OMSetRenderTargets((UINT)num, rtvHandles, false, dsv != DepthStencilView_t::INVALID ? & dsvHandle : nullptr);
}

void CommandList::SetViewports(const Viewport* const vps, size_t num)
{
	Impl->CL.DxCl->RSSetViewports((UINT)num, (D3D12_VIEWPORT*)vps);
}

void CommandList::SetDefaultScissor()
{
	D3D12_RECT rect = {};
	rect.left = 0;
	rect.right = LONG_MAX;
	rect.top = 0;
	rect.bottom = LONG_MAX;

	Impl->CL.DxCl->RSSetScissorRects(1u, &rect);
}

void CommandList::SetScissors(const ScissorRect* const scissors, size_t num)
{
	Impl->CL.DxCl->RSSetScissorRects((UINT)num, (D3D12_RECT*)scissors);
}

void CommandList::SetPipelineState(GraphicsPipelineState_t pso)
{
	if (pso != Impl->LastPipeline)
	{
		Impl->LastPipeline = pso;
		Impl->LastComputePipeline = {};
		Impl->LastRTPipeline = {};
		
		if (Dx12GraphicsPipelineStateDesc* dxPso = Dx12_GetPipelineState(pso))
		{
			Impl->CL.DxCl->IASetPrimitiveTopology(dxPso->PrimTopo);

			Impl->CL.DxCl->SetPipelineState(dxPso->PSO.Get());
		}
	}
}

void CommandList::SetPipelineState(ComputePipelineState_t pso)
{
	if (pso != Impl->LastComputePipeline)
	{
		Impl->LastComputePipeline = pso;
		Impl->LastRTPipeline = {};
		Impl->LastPipeline = {};		

		if (ID3D12PipelineState* dxPso = Dx12_GetPipelineState(pso))
		{
			Impl->CL.DxCl->SetPipelineState(dxPso);
		}
	}
}

void CommandList::SetPipelineState(RaytracingPipelineState_t pso)
{
	if(Impl->LastRTPipeline != pso)
	{
		Impl->LastRTPipeline = pso;
		Impl->LastPipeline = {};
		Impl->LastComputePipeline = {};

		if (ID3D12StateObject* dxPso = Dx12_GetPipelineState(pso))
		{
			Impl->CL.DxCl->SetPipelineState1(dxPso);
		}
	}
}

void CommandList::SetVertexBuffers(uint32_t startSlot, uint32_t count, const VertexBuffer_t* const vbs, const uint32_t* const strides, const uint32_t* const offsets)
{
	for (uint32_t i = 0; i < count; i++)
	{
		SetVertexBuffer(i + startSlot, vbs[i], strides[i], offsets[i]);
	}
}

void CommandList::SetVertexBuffers(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const vbs, const uint32_t* const strides, const uint32_t* const offsets)
{
	for (uint32_t i = 0; i < count; i++)
	{
		SetVertexBuffer(i + startSlot, vbs[i], strides[i], offsets[i]);
	}
}

void CommandList::SetVertexBuffer(uint32_t slot, VertexBuffer_t vb, uint32_t stride, uint32_t offset)
{
	D3D12_VERTEX_BUFFER_VIEW vbv = Dx12_GetVertexBufferView(vb, offset, stride);
	Impl->CL.DxCl->IASetVertexBuffers(slot, 1u, &vbv);
}

void CommandList::SetVertexBuffer(uint32_t slot, DynamicBuffer_t vb, uint32_t stride, uint32_t offset)
{
	D3D12_VERTEX_BUFFER_VIEW vbv = Dx12_GetVertexBufferView(vb, offset, stride);
	Impl->CL.DxCl->IASetVertexBuffers(slot, 1u, &vbv);
}

void CommandList::SetIndexBuffer(IndexBuffer_t ib, RenderFormat format, uint32_t indexOffset)
{
	D3D12_INDEX_BUFFER_VIEW ibv = Dx12_GetIndexBufferView(ib, format, indexOffset);
	Impl->CL.DxCl->IASetIndexBuffer(&ibv);
}

void CommandList::SetIndexBuffer(DynamicBuffer_t ib, RenderFormat format, uint32_t indexOffset)
{
	D3D12_INDEX_BUFFER_VIEW ibv = Dx12_GetIndexBufferView(ib, format, indexOffset);
	Impl->CL.DxCl->IASetIndexBuffer(&ibv);
}

void CommandList::CopyTexture(Texture_t dst, Texture_t src)
{
	ID3D12Resource* dstRes = Dx12_GetTextureResource(dst);
	ID3D12Resource* srcRes = Dx12_GetTextureResource(src);

	Impl->CL.DxCl->CopyResource(dstRes, srcRes);
}

void CommandList::DrawIndexedInstanced(uint32_t numIndices, uint32_t numInstances, uint32_t startIndex, uint32_t startVertex, uint32_t startInstance)
{
	Impl->CL.DxCl->DrawIndexedInstanced((UINT)numIndices, (UINT)numInstances, (UINT)startIndex, (UINT)startVertex, (UINT)startInstance);
}

void CommandList::DrawInstanced(uint32_t numVerts, uint32_t numInstances, uint32_t startVertex, uint32_t startInstance)
{
	Impl->CL.DxCl->DrawInstanced((UINT)numVerts, (UINT)numInstances, (UINT)startVertex, (UINT)startInstance);
}

void CommandList::ExecuteIndirect(IndirectCommand_t ic, StructuredBuffer_t argBuf, uint64_t argBufferOffset)
{
	ID3D12CommandSignature* dxCommandSig = Dx12_GetCommandSignature(ic);
	ID3D12Resource* dxArgRes = Dx12_GetBufferResource(argBuf);
	Impl->CL.DxCl->ExecuteIndirect(dxCommandSig, 1u, dxArgRes, (UINT64)argBufferOffset, nullptr, 0u);
}

void CommandList::Dispatch(uint32_t x, uint32_t y, uint32_t z)
{
	Impl->CL.DxCl->Dispatch((UINT)x, (UINT)y, (UINT)z);
}

void CommandList::DispatchMesh(uint32_t x, uint32_t y, uint32_t z) 
{ 
	Impl->CL.DxCl->DispatchMesh((UINT)x, (UINT)y, (UINT)z);
}

void CommandList::BuildRaytracingScene(RaytracingScene_t scene)
{
	Dx12_BuildRaytracingScene(this, scene);
}

void CommandList::DispatchRays(RaytracingShaderTable_t ShaderTable, uint32_t X, uint32_t Y, uint32_t Z)
{
	D3D12_DISPATCH_RAYS_DESC DxRayDesc = Dx12_GetDispatchRaysDesc(ShaderTable, X, Y, Z);

	Impl->CL.DxCl->DispatchRays(&DxRayDesc);
}

// Dx11 Style Bind Commands, Dx12 uses bindless
void CommandList::BindVertexSRVs(uint32_t startSlot, uint32_t count, const ShaderResourceView_t* const srvs) { assert("Dx12 uses bindless mode"); }
void CommandList::BindVertexCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs) { assert("Dx12 uses bindless mode"); }
void CommandList::BindVertexCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs) { assert("Dx12 uses bindless mode"); }
void CommandList::BindGeometryCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs) { assert("Dx12 uses bindless mode"); }
void CommandList::BindGeometryCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs) { assert("Dx12 uses bindless mode"); }
void CommandList::BindPixelSRVs(uint32_t startSlot, uint32_t count, const ShaderResourceView_t* const srvs) { assert("Dx12 uses bindless mode"); }
void CommandList::BindPixelCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs) { assert("Dx12 uses bindless mode"); }
void CommandList::BindPixelCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs) { assert("Dx12 uses bindless mode"); }
void CommandList::BindComputeSRVs(uint32_t startSlot, uint32_t count, const ShaderResourceView_t* const srvs) { assert("Dx12 uses bindless mode"); }
void CommandList::BindComputeUAVs(uint32_t startSlot, uint32_t count, const UnorderedAccessView_t* const srvs) { assert("Dx12 uses bindless mode"); }
void CommandList::BindComputeCBVs(uint32_t startSlot, uint32_t count, const ConstantBuffer_t* const cbvs) { assert("Dx12 uses bindless mode"); }
void CommandList::BindComputeCBVs(uint32_t startSlot, uint32_t count, const DynamicBuffer_t* const cbvs) { assert("Dx12 uses bindless mode"); }

void CommandList::SetGraphicsRootValue(uint32_t slot, uint32_t offset, uint32_t value)
{
	Impl->CL.DxCl->SetGraphicsRoot32BitConstant(slot, value, offset);
}

void CommandList::SetComputeRootValue(uint32_t slot, uint32_t offset, uint32_t value)
{
	Impl->CL.DxCl->SetComputeRoot32BitConstant(slot, value, offset);
}

void CommandList::SetGraphicsRootCBV(uint32_t slot, ConstantBuffer_t cb)
{
	Impl->CL.DxCl->SetGraphicsRootConstantBufferView(slot, Dx12_GetCbvAddress(cb));
}

void CommandList::SetComputeRootCBV(uint32_t slot, ConstantBuffer_t cb)
{
	Impl->CL.DxCl->SetComputeRootConstantBufferView(slot, Dx12_GetCbvAddress(cb));
}

void CommandList::SetGraphicsRootCBV(uint32_t slot, DynamicBuffer_t cb)
{
	Impl->CL.DxCl->SetGraphicsRootConstantBufferView(slot, Dx12_GetDbAddress(cb));
}

void CommandList::SetComputeRootCBV(uint32_t slot, DynamicBuffer_t cb)
{
	Impl->CL.DxCl->SetComputeRootConstantBufferView(slot, Dx12_GetDbAddress(cb));
}

void CommandList::SetGraphicsRootSRV(uint32_t slot, ShaderResourceView_t srv)
{
	Impl->CL.DxCl->SetGraphicsRootShaderResourceView(slot, Impl->SrvGpuAdress(srv));
}

void CommandList::SetComputeRootSRV(uint32_t slot, ShaderResourceView_t srv)
{
	Impl->CL.DxCl->SetComputeRootShaderResourceView(slot, Impl->SrvGpuAdress(srv));
}

void CommandList::SetComputeRootSRV(uint32_t slot, RaytracingScene_t srv)
{
	if (ID3D12Resource* dxScene = Dx12_GetRaytracingScene(srv))
	{
		Impl->CL.DxCl->SetComputeRootShaderResourceView(slot, dxScene->GetGPUVirtualAddress());
	}
}

void CommandList::SetGraphicsRootUAV(uint32_t slot, UnorderedAccessView_t uav)
{
	Impl->CL.DxCl->SetGraphicsRootUnorderedAccessView(slot, Impl->UavGpuAdress(uav));
}

void CommandList::SetComputeRootUAV(uint32_t slot, UnorderedAccessView_t uav)
{
	Impl->CL.DxCl->SetComputeRootUnorderedAccessView(slot, Impl->UavGpuAdress(uav));
}

void CommandList::SetGraphicsRootDescriptorTable(uint32_t slot)
{
	Impl->CL.DxCl->SetGraphicsRootDescriptorTable(slot, Impl->SrvUavTableDescriptorHandle());
}

void CommandList::SetComputeRootDescriptorTable(uint32_t slot)
{
	Impl->CL.DxCl->SetComputeRootDescriptorTable(slot, Impl->SrvUavTableDescriptorHandle());
}

void CommandList::TransitionResource(Texture_t tex, ResourceTransitionState before, ResourceTransitionState after)
{
	D3D12_RESOURCE_BARRIER barrier = Dx12_TransitionBarrier(
		Dx12_GetTextureResource(tex), 
		Dx12_ResourceState(before), 
		Dx12_ResourceState(after)
	);

	Impl->CL.DxCl->ResourceBarrier(1u, &barrier);
}

void CommandList::TransitionResource(StructuredBuffer_t buf, ResourceTransitionState before, ResourceTransitionState after)
{
	D3D12_RESOURCE_BARRIER barrier = Dx12_TransitionBarrier(
		Dx12_GetBufferResource(buf),
		Dx12_ResourceState(before),
		Dx12_ResourceState(after)
	);

	Impl->CL.DxCl->ResourceBarrier(1u, &barrier);
}

void CommandList::UAVBarrier(Texture_t tex)
{
	D3D12_RESOURCE_BARRIER barrier = Dx12_UavBarrier(Dx12_GetTextureResource(tex));

	Impl->CL.DxCl->ResourceBarrier(1u, &barrier);
}

void CommandList::UAVBarrier(StructuredBuffer_t buf)
{
	D3D12_RESOURCE_BARRIER barrier = Dx12_UavBarrier(Dx12_GetBufferResource(buf));

	Impl->CL.DxCl->ResourceBarrier(1u, &barrier);
}

void CommandList::BeginEvent(const char* EventStr) 
{
	assert(EventStr);
	Impl->CL.DxCl->BeginEvent(1, EventStr, static_cast<UINT>(strlen(EventStr)) + 1);
}

void CommandList::BeginEvent(const wchar_t* EventStr)
{
	assert(EventStr);
	Impl->CL.DxCl->BeginEvent(0, EventStr, static_cast<UINT>(wcslen(EventStr)) + 1);
}

void CommandList::EndEvent()
{
	Impl->CL.DxCl->EndEvent();
}

void CommandList::AddMarker(const char* MarkerStr)
{
	assert(MarkerStr);
	Impl->CL.DxCl->SetMarker(1, MarkerStr, static_cast<UINT>(strlen(MarkerStr)) + 1);
}

void CommandList::AddMarker(const wchar_t* MarkerStr)
{
	assert(MarkerStr);
	Impl->CL.DxCl->SetMarker(0, MarkerStr, static_cast<UINT>(wcslen(MarkerStr)) + 1);
}

CommandListPtr CommandList::Create()
{
	return Create(CommandListType::GRAPHICS);
}

CommandListPtr CommandList::Create(CommandListType type)
{
	return std::shared_ptr<CommandList>(CreateRaw(type));
}

CommandList* CommandList::CreateRaw(CommandListType type)
{
	CommandListImpl* cli = new CommandListImpl(Dx12_AccquireCommandList(type));

	CommandList* cl = new CommandList(cli, type);

	cl->Begin();

	return cl;
}

void CommandList::Execute(CommandListPtr& cl)
{
	cl->Finish();

	Dx12CommandQueue* queue = Dx12_GetCommandQueue(cl->Type);	

	ID3D12CommandList* cls[] = { cl->Impl->CL.DxCl.Get() };
	queue->DxCommandQueue->ExecuteCommandLists(1u, cls);

	const uint64_t fenceValue = Dx12_Signal(cl->Type);

	cl->Impl->CL.Allocator.FenceValue = fenceValue;

	cl->Impl->SubmitHeaps(cl->Type, fenceValue);
}

void CommandList::ExecuteAndStall(CommandListPtr& cl)
{
	Execute(cl);

	Dx12_Wait(cl->Type, cl->Impl->CL.Allocator.FenceValue);
}

void CommandList::ReleaseAll()
{

}

CommandListImpl* CommandList::GetCommandListImpl()
{
	return Impl.get();
}

ID3D12GraphicsCommandList* Dx12_GetCommandList(CommandList* cl)
{
	return cl->GetCommandListImpl()->CL.DxCl.Get();
}

CommandList* CommandListSubmissionGroup::CreateCommandList()
{
	CommandLists.emplace_back(std::unique_ptr<CommandList>(CommandList::CreateRaw(Type)));

	return CommandLists.back().get();
}

void CommandListSubmissionGroup::Submit()
{
	std::vector<ID3D12GraphicsCommandList*> dxCls;
	dxCls.resize(CommandLists.size());

	for (size_t i = 0; i < CommandLists.size(); i++)
	{
		dxCls[i] = Dx12_GetCommandList(CommandLists[i].get());

		dxCls[i]->Close();
	}

	Dx12CommandQueue* queue = Dx12_GetCommandQueue(Type);

	queue->DxCommandQueue->ExecuteCommandLists((UINT)dxCls.size(), (ID3D12CommandList**)dxCls.data());
	
	const uint64_t fenceVaue = Dx12_Signal(Type);

	for (size_t i = 0; i < CommandLists.size(); i++)
	{
		CommandLists[i]->GetCommandListImpl()->CL.Allocator.FenceValue = fenceVaue;

		CommandLists[i]->GetCommandListImpl()->SubmitHeaps(Type, fenceVaue);
	}
}

}
