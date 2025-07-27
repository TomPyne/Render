#include "RenderImpl.h"

#include "Render.h"
#include "RenderDefines.h"
#include "Buffers.h"

#include <dxgi1_6.h>

#include <mutex>

namespace rl
{

Dx12RenderGlobals g_render;

ComPtr<IDXGIAdapter> EnumerateAdapters(bool debug)
{
	ComPtr<IDXGIFactory6> dxgiFactory;

	UINT factoryFlags = debug ? DXGI_CREATE_FACTORY_DEBUG : 0u;

	if (!DXENSURE(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&dxgiFactory))))
		return nullptr;	

	ComPtr<IDXGIAdapter> dxgiAdapter;

	if (!DXENSURE(dxgiFactory->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&dxgiAdapter))))
		return nullptr;

	return dxgiAdapter;
}

ComPtr<ID3D12Device5> CreateDevice(bool debug)
{
	g_render.Debug = debug;

	if (debug)
	{
		ComPtr<ID3D12Debug1> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
			debugController->SetEnableGPUBasedValidation(TRUE);
		}
	}

	ComPtr<IDXGIAdapter> adapter = EnumerateAdapters(debug);

	ComPtr<ID3D12Device5> device;
	if (!DXENSURE(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_12_1, IID_PPV_ARGS(&device))))
		return nullptr;

	if (debug)
	{
		// TODO: Debug message handling?
		// Or make this user customisable via push/pop
		ComPtr<ID3D12InfoQueue> pInfoQueue;
		if (SUCCEEDED(device.As(&pInfoQueue)))
		{
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
			pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

			// Suppress whole categories of messages
			//D3D12_MESSAGE_CATEGORY Categories[] = 
			//{ 
			//};

			// Suppress messages based on their severity level
			D3D12_MESSAGE_SEVERITY Severities[] =
			{
				D3D12_MESSAGE_SEVERITY_INFO,
			};

			// Suppress individual messages by their ID
			D3D12_MESSAGE_ID DenyIds[] = {
				D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,                         // This warning occurs when using capture frame while graphics debugging.
				D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,                       // This warning occurs when using capture frame while graphics debugging.
				D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
				D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
				//D3D12_MESSAGE_ID_COMMAND_LIST_STATIC_DESCRIPTOR_RESOURCE_DIMENSION_MISMATCH,
				//D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
				//D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_RENDERTARGETVIEW_NOT_SET,
				//D3D12_MESSAGE_ID_RENDER_TARGET_FORMAT_MISMATCH_PIPELINE_STATE
				
			};

			D3D12_INFO_QUEUE_FILTER NewFilter = {};
			//NewFilter.DenyList.NumCategories = _countof(Categories);
			//NewFilter.DenyList.pCategoryList = Categories;
			NewFilter.DenyList.NumSeverities = _countof(Severities);
			NewFilter.DenyList.pSeverityList = Severities;
			NewFilter.DenyList.NumIDs = _countof(DenyIds);
			NewFilter.DenyList.pIDList = DenyIds;

			DXENSURE(pInfoQueue->PushStorageFilter(&NewFilter));
		}
	}	

	return device;
}

D3D12_COMMAND_QUEUE_DESC CreateCommandQueueDesc(D3D12_COMMAND_LIST_TYPE type)
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.NodeMask = 0u;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

	return desc;
}

ComPtr<ID3D12CommandQueue> CreateDxCommandQueue(D3D12_COMMAND_LIST_TYPE type)
{
	D3D12_COMMAND_QUEUE_DESC desc = CreateCommandQueueDesc(type);

	ComPtr<ID3D12CommandQueue> dxCommandQueue;

	if (!DXENSURE(g_render.DxDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&dxCommandQueue))))
		return {};

	return dxCommandQueue;
}

ComPtr<ID3D12Fence> Dx12_CreateFence(uint64_t fenceValue)
{
	ComPtr<ID3D12Fence> dxFence;

	if (!DXENSURE(g_render.DxDevice->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&dxFence))))
		return {};

	return dxFence;
}

void InitCommandQueue(Dx12CommandQueue& Queue, D3D12_COMMAND_LIST_TYPE Type)
{
	Queue.DxCommandQueue = CreateDxCommandQueue(Type);
	Queue.DxFence = Dx12_CreateFence(0);
	Queue.FenceEventHandle = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	Queue.FenceValue = 0;
}

bool Render_Init(const RenderInitParams& params)
{
	g_render.DxDevice = CreateDevice(params.DebugEnabled);

	if (!g_render.DxDevice)
	{
		return false;
	}

	// Query feature support

	// Mesh shaders
	{
		D3D12_FEATURE_DATA_SHADER_MODEL ShaderModel = { D3D_SHADER_MODEL_6_5 };
		if (SUCCEEDED(g_render.DxDevice->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &ShaderModel, sizeof(ShaderModel)))
			|| (ShaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_5))
		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS7 Features = {};
			if (SUCCEEDED(g_render.DxDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &Features, sizeof(Features)))
				|| (Features.MeshShaderTier == D3D12_MESH_SHADER_TIER_NOT_SUPPORTED))
			{
				g_render.SupportsMeshShaders = true;
			}
			else
			{
				OutputDebugStringA("Mesh shaders: Feature unsupported by hardware\n");
			}
		}
		else
		{
			OutputDebugStringA("Mesh shaders: Shader Model 6.5 is not supported\n");
		}
	}

	// Ray tracing

	{
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 Features = {};
		if (SUCCEEDED(g_render.DxDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &Features, sizeof(Features))))
		{
			g_render.SupportsRaytracing = Features.RaytracingTier >= D3D12_RAYTRACING_TIER_1_0;
		}

		if (!g_render.SupportsRaytracing)
		{
			OutputDebugStringA("Ray tracing: Feature unsupported by hardware\n");
		}
	}

	g_render.RootSignature = CreateRootSignature(params.RootSigDesc);

	InitCommandQueue(g_render.DirectQueue, D3D12_COMMAND_LIST_TYPE_DIRECT);
	InitCommandQueue(g_render.ComputeQueue, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	InitCommandQueue(g_render.CopyQueue, D3D12_COMMAND_LIST_TYPE_COPY);

	return true;
}

bool Render_Initialised()
{
	return g_render.DxDevice != nullptr;
}

void Render_BeginFrame()
{
	DynamicBuffers_NewFrame();
}

void Render_BeginRenderFrame()
{
	Dx12_DescriptorsBeginFrame();
	Dx12_TexturesBeginFrame();
}

void Render_EndFrame()
{
	DynamicBuffers_EndFrame();
}

void Render_ShutDown()
{
	g_render.DxDevice = nullptr;

	// RHI TODO: release all device resources if we shutdown the renderer and dont immediately close the program.
}

void Render_PushDebugWarningDisable(RenderDebugWarnings warning)
{
}

void Render_PopDebugWarningDisable()
{
}

bool Render_IsBindless()
{
	return true;
}

bool Render_SupportsMeshShaders()
{
	return g_render.SupportsMeshShaders;
}

bool Render_SupportsRaytracing()
{
	return g_render.SupportsRaytracing;
}

bool Render_IsThreadSafe()
{
	return RENDER_THREAD_SAFE;
}

const char* Render_ApiId()
{
	return "RAPI_DX12";
}

D3D12_COMMAND_LIST_TYPE Dx12_CommandListType(CommandListType type)
{
	switch (type)
	{
	case CommandListType::GRAPHICS: return D3D12_COMMAND_LIST_TYPE_DIRECT;
	case CommandListType::COMPUTE: return D3D12_COMMAND_LIST_TYPE_COMPUTE;
	case CommandListType::COPY: return D3D12_COMMAND_LIST_TYPE_COPY;
	}

	assert(0 && "Dx12_CommandListType invalid type");
	return (D3D12_COMMAND_LIST_TYPE)0;
}

Dx12CommandQueue* Dx12_GetCommandQueue(CommandListType type)
{
	switch (type)
	{
	case CommandListType::GRAPHICS:	return &g_render.DirectQueue;
	case CommandListType::COMPUTE:	return &g_render.ComputeQueue;
	case CommandListType::COPY:		return &g_render.CopyQueue;
	}

	assert(0 && "Dx12_Signal invalid queue");
	return nullptr;
}

uint64_t Dx12_Signal(CommandListType queue)
{
	Dx12CommandQueue* commandQueue = Dx12_GetCommandQueue(queue);

	commandQueue->FenceValue++;

	uint64_t value = commandQueue->FenceValue.load();	

	DXENSURE(commandQueue->DxCommandQueue->Signal(commandQueue->DxFence.Get(), value));

	return value;
}

void Dx12_SignalFence(ID3D12Fence* dxFence, CommandListType queue, uint64_t value)
{
	Dx12CommandQueue* commandQueue = Dx12_GetCommandQueue(queue);

	DXENSURE(commandQueue->DxCommandQueue->Signal(dxFence, value));
}

void Dx12_Wait(CommandListType queue, uint64_t value)
{
	Dx12CommandQueue* commandQueue = Dx12_GetCommandQueue(queue);

	DXENSURE(commandQueue->DxFence->SetEventOnCompletion(value, nullptr));
}

D3D12_RESOURCE_STATES Dx12_ResourceState(ResourceTransitionState state)
{
	switch (state)
	{
	// present and common are synonymous in d3d12
	case ResourceTransitionState::COMMON: return D3D12_RESOURCE_STATE_COMMON;
	//case ResourceTransitionState::PRESENT: return D3D12_RESOURCE_STATE_PRESENT;
	case ResourceTransitionState::VERTEX_CONSTANT_BUFFER: return D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	case ResourceTransitionState::INDEX_BUFFER: return D3D12_RESOURCE_STATE_INDEX_BUFFER;
	case ResourceTransitionState::RENDER_TARGET: return D3D12_RESOURCE_STATE_RENDER_TARGET;
	case ResourceTransitionState::UNORDERED_ACCESS: return D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	case ResourceTransitionState::DEPTH_WRITE: return D3D12_RESOURCE_STATE_DEPTH_WRITE;
	case ResourceTransitionState::DEPTH_READ: return D3D12_RESOURCE_STATE_DEPTH_READ;
	case ResourceTransitionState::NON_PIXEL_SHADER_RESOURCE: return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	case ResourceTransitionState::PIXEL_SHADER_RESOURCE: return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	case ResourceTransitionState::INDIRECT_ARGUMENT: return D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	case ResourceTransitionState::COPY_DEST: return D3D12_RESOURCE_STATE_COPY_DEST;
	case ResourceTransitionState::COPY_SRC: return D3D12_RESOURCE_STATE_COPY_SOURCE;
	case ResourceTransitionState::READ: return D3D12_RESOURCE_STATE_GENERIC_READ;	
	case ResourceTransitionState::ALL_SHADER_RESOURCE: return D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;	
	}

	assert(0 && "Dx12_ResourceState unsupported state");

	return (D3D12_RESOURCE_STATES)0;
}

D3D12_HEAP_PROPERTIES Dx12_HeapProps(D3D12_HEAP_TYPE type)
{
	D3D12_HEAP_PROPERTIES props = {};

	props.Type = type;
	props.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	props.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	props.CreationNodeMask = 0;
	props.VisibleNodeMask = 0;

	return props;
}

D3D12_RESOURCE_DESC Dx12_BufferDesc(size_t size, D3D12_RESOURCE_FLAGS flags)
{
	D3D12_RESOURCE_DESC desc = {};
	desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	desc.Alignment = 0;
	desc.Width = (UINT)size;
	desc.Height = 1u;
	desc.DepthOrArraySize = 1u;
	desc.MipLevels = 1u;
	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.SampleDesc.Count = 1u;
	desc.SampleDesc.Quality = 0u;
	desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	desc.Flags = flags;

	return desc;
}

ComPtr<ID3D12Resource> Dx12_CreateBuffer(size_t size, D3D12_HEAP_TYPE heapType, D3D12_RESOURCE_STATES initialState, D3D12_RESOURCE_FLAGS flags)
{
	ComPtr<ID3D12Resource> res = nullptr;

	const D3D12_HEAP_PROPERTIES props = Dx12_HeapProps(heapType);

	const D3D12_RESOURCE_DESC desc = Dx12_BufferDesc(size, flags);

	DXENSURE(g_render.DxDevice->CreateCommittedResource(
		&props,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		initialState,
		nullptr,
		IID_PPV_ARGS(&res)
	));

	return res;
}

}