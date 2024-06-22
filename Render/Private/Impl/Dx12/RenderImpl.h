#pragma once

#include "RenderTypes.h"
#include "Shaders.h"
#include "Dx12Types.h"

struct IDxcBlob;

namespace tpr
{

FWD_RENDER_TYPE(VertexShader_t);
FWD_RENDER_TYPE(PixelShader_t);
FWD_RENDER_TYPE(GeometryShader_t);
FWD_RENDER_TYPE(ComputeShader_t);

FWD_RENDER_TYPE(VertexBuffer_t);
FWD_RENDER_TYPE(IndexBuffer_t);
FWD_RENDER_TYPE(StructuredBuffer_t);
FWD_RENDER_TYPE(ConstantBuffer_t);
FWD_RENDER_TYPE(DynamicBuffer_t);

FWD_RENDER_TYPE(ShaderResourceView_t);
FWD_RENDER_TYPE(UnorderedAccessView_t);
FWD_RENDER_TYPE(RenderTargetView_t);
FWD_RENDER_TYPE(DepthStencilView_t);

FWD_RENDER_TYPE(Texture_t);

FWD_RENDER_TYPE(GraphicsPipelineState_t);
FWD_RENDER_TYPE(ComputePipelineState_t);

struct Dx12CommandQueue
{
	ComPtr<ID3D12CommandQueue> DxCommandQueue;
	ComPtr<ID3D12Fence> DxFence;
	uint64_t FenceValue = 0u;
};

struct Dx12CommandAllocator
{
	ComPtr<ID3D12CommandAllocator> DxAllocator;
	UINT64 FenceValue = 0u;
};

struct Dx12CommandList
{
	D3D12_COMMAND_LIST_TYPE DxType = D3D12_COMMAND_LIST_TYPE_DIRECT;
	Dx12CommandAllocator Allocator;
	ComPtr<ID3D12GraphicsCommandList> DxCl;
};

struct Dx12DescriptorHeap
{
	ComPtr<ID3D12DescriptorHeap> DxHeap;
	uint64_t DirectFenceValue = 0u;
	uint64_t ComputeFenceValue = 0u;
};

struct Dx12RenderGlobals
{
	ComPtr<ID3D12Device> DxDevice;

	Dx12CommandQueue DirectQueue;
	Dx12CommandQueue ComputeQueue;
	Dx12CommandQueue CopyQueue;

	RootSignature_t RootSignature = RootSignature_t::INVALID;

	ComPtr<ID3D12Fence> DxFrameFence;
	ComPtr<ID3D12Fence> DxComputeFence;

	uint64_t FrameFenceValue = 0;
	uint64_t ComputeFenceValue = 0;

	bool Debug = false;
};

extern Dx12RenderGlobals g_render;

uint64_t Dx12_FlushQueue(Dx12CommandQueue& queue);

uint64_t Dx12_EndGraphicsFrame();
uint64_t Dx12_EndComputeFrame();

IDxcBlob* Dx12_GetVertexShaderBlob(VertexShader_t vs);
IDxcBlob* Dx12_GetPixelShaderBlob(PixelShader_t ps);
IDxcBlob* Dx12_GetGeometryShaderBlob(GeometryShader_t gs);
IDxcBlob* Dx12_GetComputeShaderBlob(ComputeShader_t cs);

Dx12CommandList Dx12_AccquireCommandList(CommandListType type);
Dx12CommandList Dx12_AccquireCommandList(D3D12_COMMAND_LIST_TYPE type);
ID3D12GraphicsCommandList* Dx12_GetCommandList(CommandList* cl);

void Dx12_DescriptorsBeginFrame();

void Dx12_TexturesBeginFrame();
void Dx12_TexturesProcessPendingDeletes(bool flush);
void Dx12_TexturesMarkAsUsedByQueue(Texture_t tex, CommandListType type, uint64_t fenceValue);
ID3D12Resource* Dx12_GetTextureResource(Texture_t tex);

Dx12DescriptorHeap Dx12_AccquireSrvUavHeap();
Dx12DescriptorHeap Dx12_AccquireRtvHeap();
Dx12DescriptorHeap Dx12_AccquireDsvHeap();

void Dx12_SubmitSrvUavDescriptorHeap(Dx12DescriptorHeap&& heap, uint64_t graphicsFenceValue, uint64_t computeFenceValue);
void Dx12_SubmitRtvDescriptorHeap(Dx12DescriptorHeap&& heap, uint64_t graphicsFenceValue, uint64_t computeFenceValue);
void Dx12_SubmitDsvDescriptorHeap(Dx12DescriptorHeap&& heap, uint64_t graphicsFenceValue, uint64_t computeFenceValue);

D3D12_CPU_DESCRIPTOR_HANDLE Dx12_RtvDescriptorHandle(ID3D12DescriptorHeap* heap, RenderTargetView_t rtv);
D3D12_CPU_DESCRIPTOR_HANDLE Dx12_DsvDescriptorHandle(ID3D12DescriptorHeap* heap, DepthStencilView_t dsv);

ID3D12PipelineState* Dx12_GetPipelineState(GraphicsPipelineState_t pso);
ID3D12PipelineState* Dx12_GetPipelineState(ComputePipelineState_t pso);

D3D12_VERTEX_BUFFER_VIEW Dx12_GetVertexBufferView(VertexBuffer_t vb, uint32_t offset, uint32_t stride);
D3D12_VERTEX_BUFFER_VIEW Dx12_GetVertexBufferView(DynamicBuffer_t db, uint32_t offset, uint32_t stride);
D3D12_INDEX_BUFFER_VIEW Dx12_GetIndexBufferView(IndexBuffer_t ib, RenderFormat format, uint32_t offset);
D3D12_INDEX_BUFFER_VIEW Dx12_GetIndexBufferView(DynamicBuffer_t db, RenderFormat format, uint32_t offset);

D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetCbvAddress(ConstantBuffer_t cb);
D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetCbvAddress(DynamicBuffer_t db);
D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetSrvAddress(ID3D12DescriptorHeap* heap, ShaderResourceView_t srv);
D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetUavAddress(ID3D12DescriptorHeap* heap, UnorderedAccessView_t uav);
D3D12_GPU_DESCRIPTOR_HANDLE Dx12_GetSrvUavTableHandle(ID3D12DescriptorHeap* heap);

D3D12_RESOURCE_STATES Dx12_ResourceState(ResourceTransitionState state);

D3D12_RESOURCE_BARRIER Dx12_TransitionBarrier(ID3D12Resource* pRes, D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after);
D3D12_RESOURCE_BARRIER Dx12_UavBarrier(ID3D12Resource* pRes);

void Dx12_SetTextureResource(Texture_t tex, const ComPtr<ID3D12Resource>& resource);

D3D12_COMMAND_LIST_TYPE Dx12_CommandListType(CommandListType type);

Dx12CommandQueue* Dx12_GetCommandQueue(CommandListType type);
uint64_t Dx12_Signal(CommandListType queue);
void Dx12_Wait(CommandListType queue, uint64_t value);

D3D12_HEAP_PROPERTIES Dx12_HeapProps(D3D12_HEAP_TYPE type);
D3D12_RESOURCE_DESC Dx12_BufferDesc(size_t size, D3D12_RESOURCE_FLAGS flags);
ComPtr<ID3D12Resource> Dx12_CreateBuffer(size_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState, D3D12_RESOURCE_FLAGS flags);

ID3D12RootSignature* Dx12_GetRootSignature(RootSignature_t rs);

}
