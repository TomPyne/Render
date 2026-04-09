#pragma once

#include "RenderTypes.h"
#include "Shaders.h"
#include "Dx12Types.h"

struct IDxcBlob;

namespace rl
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
	HANDLE FenceEventHandle;
	std::atomic<uint64_t> FenceValue = 0u;
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
	ComPtr<ID3D12GraphicsCommandList6> DxCl;
};

struct Dx12DescriptorHeap
{
	ComPtr<ID3D12DescriptorHeap> DxHeap;
	uint64_t DirectFenceValue = 0u;
	uint64_t ComputeFenceValue = 0u;
};

struct Dx12GraphicsPipelineStateDesc
{
	ComPtr<ID3D12PipelineState> PSO = nullptr;
	D3D12_PRIMITIVE_TOPOLOGY PrimTopo = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
};

struct Dx12RenderGlobals
{
	ComPtr<ID3D12Device5> DxDevice;

	Dx12CommandQueue DirectQueue;
	Dx12CommandQueue ComputeQueue;
	Dx12CommandQueue CopyQueue;

	RootSignature_t RootSignature = RootSignature_t::INVALID;

	bool Debug = false;
	bool SupportsMeshShaders = false;
	bool SupportsRaytracing = false;
};

struct Dx12StaticBufferAllocation
{
	D3D12_GPU_VIRTUAL_ADDRESS pGPUMem = D3D12_GPU_VIRTUAL_ADDRESS{ 0 };

	size_t Offset = 0u;
	size_t Size = 0u;

	ID3D12Resource* pResource;

	bool SingleBuffer = false;
};

extern Dx12RenderGlobals g_render;

void Dx12_FlushQueue(Dx12CommandQueue& queue);
void Dx12_FlushQueues();

IDxcBlob* Dx12_GetVertexShaderBlob(VertexShader_t vs);
IDxcBlob* Dx12_GetPixelShaderBlob(PixelShader_t ps);
IDxcBlob* Dx12_GetGeometryShaderBlob(GeometryShader_t gs);
IDxcBlob* Dx12_GetMeshShaderBlob(MeshShader_t ms);
IDxcBlob* Dx12_GetAmplificationShaderBlob(AmplificationShader_t as);
IDxcBlob* Dx12_GetComputeShaderBlob(ComputeShader_t cs);
IDxcBlob* Dx12_GetRayGenShaderBlob(RaytracingRayGenShader_t rgs);
IDxcBlob* Dx12_GetRayMissShaderBlob(RaytracingMissShader_t rms);

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

// TODO: Switch all pipelines to be state objects
Dx12GraphicsPipelineStateDesc* Dx12_GetPipelineState(GraphicsPipelineState_t pso);
ID3D12PipelineState* Dx12_GetPipelineState(ComputePipelineState_t pso);
ID3D12StateObject* Dx12_GetPipelineState(RaytracingPipelineState_t pso);

// It is up to the calling code to free this memory reponsibly
Dx12StaticBufferAllocation Dx12_CreateByteBuffer(const void* const Data, size_t Size, size_t Alignment = D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT);
Dx12StaticBufferAllocation Dx12_CreateRWByteBuffer(const void* const Data, size_t Size);
D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetByteBufferAddress(const Dx12StaticBufferAllocation& Alloc);
D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetByteBufferSize(const Dx12StaticBufferAllocation& Alloc);
void Dx12_FreeByteBuffer(const Dx12StaticBufferAllocation& Alloc);

D3D12_VERTEX_BUFFER_VIEW Dx12_GetVertexBufferView(VertexBuffer_t vb, uint32_t offset, uint32_t stride);
D3D12_VERTEX_BUFFER_VIEW Dx12_GetVertexBufferView(DynamicBuffer_t db, uint32_t offset, uint32_t stride);
D3D12_INDEX_BUFFER_VIEW Dx12_GetIndexBufferView(IndexBuffer_t ib, RenderFormat format, uint32_t offset);
D3D12_INDEX_BUFFER_VIEW Dx12_GetIndexBufferView(DynamicBuffer_t db, RenderFormat format, uint32_t offset);
ID3D12Resource* Dx12_GetDynamicBufferResource(DynamicBuffer_t db, size_t* outOffset);

D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetCbvAddress(ConstantBuffer_t cb);
D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetDbAddress(DynamicBuffer_t db);
D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetVbAddress(VertexBuffer_t vb);
D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetIbAddress(IndexBuffer_t ib);
D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetSbAddress(StructuredBuffer_t sb);
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
void Dx12_SignalFence(ID3D12Fence* dxFence, CommandListType queue, uint64_t value);
void Dx12_Wait(CommandListType queue, uint64_t value);

D3D12_HEAP_PROPERTIES Dx12_HeapProps(D3D12_HEAP_TYPE type);
D3D12_RESOURCE_DESC Dx12_BufferDesc(size_t size, D3D12_RESOURCE_FLAGS flags);
ComPtr<ID3D12Resource> Dx12_CreateBuffer(size_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState, D3D12_RESOURCE_FLAGS flags);
ID3D12Resource* Dx12_GetBufferResource(StructuredBuffer_t buf);
uint32_t Dx12_GetBufferOffset(StructuredBuffer_t buf);

ComPtr<ID3D12Fence> Dx12_CreateFence(uint64_t fenceValue);

ID3D12RootSignature* Dx12_GetRootSignature(RootSignature_t rs);
ID3D12CommandSignature* Dx12_GetCommandSignature(IndirectCommand_t ic);

void Dx12_BuildRaytracingScene(CommandList* cl, RaytracingScene_t scene);
ID3D12StateObject* Dx12_GetRaytracingStateObject(RaytracingPipelineState_t RaytracingPipelineState);
D3D12_DISPATCH_RAYS_DESC Dx12_GetDispatchRaysDesc(RaytracingShaderTable_t ShaderTable, uint32_t X, uint32_t Y, uint32_t Z);
ID3D12Resource* Dx12_GetRaytracingScene(RaytracingScene_t scene);
}
