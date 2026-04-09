#include "Impl/BindingImpl.h"

#include "RenderImpl.h"
#include "Impl/TexturesImpl.h"
#include "Textures.h"
#include "IDArray.h"

#include "SparseArray.h"

#include <queue>
#include <vector>

namespace rl
{

RENDER_TYPE(SRVUAV_t);

enum class DescriptorType
{
	SRV,
	UAV
};

struct SRVUAVDescriptor
{
	DescriptorType Type;
	union
	{
		D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc;
		D3D12_UNORDERED_ACCESS_VIEW_DESC UavDesc;
	};
	ID3D12Resource* Resource;
};

struct RTVDescriptor
{
	D3D12_RENDER_TARGET_VIEW_DESC RtvDesc;
	ID3D12Resource* Resource;
};

struct DSVDescriptor
{
	D3D12_DEPTH_STENCIL_VIEW_DESC DsvDesc;
	ID3D12Resource* Resource;
};

IDArray<SRVUAV_t, SRVUAVDescriptor> g_SrvUavDescriptors;

SparseArray<RTVDescriptor, RenderTargetView_t> g_RtvDescriptors;
std::shared_mutex g_RtvMutex;

SparseArray<DSVDescriptor, DepthStencilView_t> g_DsvDescriptors;
std::shared_mutex g_DsvMutex;

struct DescriptorHeaps
{
	UINT DescriptorHandleIncrement = 0u;

	std::vector<Dx12DescriptorHeap> FreeHeaps;
	std::queue<Dx12DescriptorHeap> PendingDeletion;
	std::queue<Dx12DescriptorHeap> InFlightHeaps;

	std::mutex Mutex;

	virtual Dx12DescriptorHeap CreateHeap() = 0;

	void BeginFrame()
	{
		auto lock = std::scoped_lock(Mutex);

		if (!InFlightHeaps.empty() || !PendingDeletion.empty())
		{
			const uint64_t directFenceCurrent = g_render.DirectQueue.DxFence->GetCompletedValue();
			const uint64_t computeFenceCurrent = g_render.ComputeQueue.DxFence->GetCompletedValue();

			while (!InFlightHeaps.empty())
			{
				if (InFlightHeaps.front().ComputeFenceValue < computeFenceCurrent && InFlightHeaps.front().DirectFenceValue < directFenceCurrent)
				{
					FreeHeaps.emplace_back(std::move(InFlightHeaps.front()));
					InFlightHeaps.pop();
				}
				else
				{
					break;
				}
			}

			while (!PendingDeletion.empty())
			{
				if (PendingDeletion.front().ComputeFenceValue <= computeFenceCurrent && PendingDeletion.front().DirectFenceValue <= directFenceCurrent)
				{
					PendingDeletion.pop();
				}
				else
				{
					break;
				}

				if (PendingDeletion.empty())
				{
					break;
				}
			}
		}
	}

	Dx12DescriptorHeap AccquireHeap()
	{
		auto lock = std::scoped_lock(Mutex);

		Dx12DescriptorHeap heap = {};

		if (!FreeHeaps.empty())
		{
			heap = std::move(FreeHeaps.back());
			FreeHeaps.pop_back();
		}
		else
		{
			heap = CreateHeap();
		}

		return heap;
	}

	void HeapChanged()
	{
		auto lock = std::scoped_lock(Mutex);

		FreeHeaps.clear();

		while(!InFlightHeaps.empty())
		{
			PendingDeletion.emplace(std::move(InFlightHeaps.front()));
			InFlightHeaps.pop();
		}
	}

	void SubmitHeap(Dx12DescriptorHeap&& heap, uint64_t graphicsFenceValue, uint64_t computeFenceValue)
	{
		auto lock = std::scoped_lock(Mutex);

		heap.DirectFenceValue = graphicsFenceValue;
		heap.ComputeFenceValue = computeFenceValue;

		InFlightHeaps.emplace(std::move(heap));
	}
};

struct SrvUavDescriptorHeaps : public DescriptorHeaps
{
	virtual Dx12DescriptorHeap CreateHeap() override
	{
		Dx12DescriptorHeap heap = {};

		auto lock = g_SrvUavDescriptors.ReadScopeLock(); // Lock here so size stays consistent between NumDescriptors and during the loop

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
		desc.NumDescriptors = (UINT)g_SrvUavDescriptors.Size();
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		desc.NodeMask = 0;

		if (!DXENSURE(g_render.DxDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.DxHeap))))
		{
			assert(0 && "SrvUavDescriptorHeaps : CreateHeap failed");

			return {};
		}

		DescriptorHandleIncrement = g_render.DxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

		D3D12_CPU_DESCRIPTOR_HANDLE handle = heap.DxHeap->GetCPUDescriptorHandleForHeapStart();

		D3D12_SHADER_RESOURCE_VIEW_DESC nullSrvDesc = {};
		nullSrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		nullSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		nullSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

		D3D12_UNORDERED_ACCESS_VIEW_DESC nullUavDesc = {};
		nullUavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		nullUavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

		g_SrvUavDescriptors.ForEachNullIfValid([&](SRVUAVDescriptor* descriptor) 
		{
			if (!descriptor)
			{
				g_render.DxDevice->CreateShaderResourceView(nullptr, &nullSrvDesc, handle);
			}
			else
			{
				if (descriptor->Type == DescriptorType::SRV)
				{
					g_render.DxDevice->CreateShaderResourceView(descriptor->Resource, descriptor->Resource ? &descriptor->SrvDesc : &nullSrvDesc, handle);
				}
				else if (descriptor->Type == DescriptorType::UAV)
				{
					g_render.DxDevice->CreateUnorderedAccessView(descriptor->Resource, nullptr, descriptor->Resource ? &descriptor->UavDesc : &nullUavDesc, handle);
				}
			}

			handle.ptr += DescriptorHandleIncrement;
		});

		return heap;
	}
};

struct RtvDescriptorHeap : public DescriptorHeaps
{
	virtual Dx12DescriptorHeap CreateHeap() override
	{
		Dx12DescriptorHeap heap = {};

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		desc.NumDescriptors = (UINT)g_RtvDescriptors.Size();
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		if (!DXENSURE(g_render.DxDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.DxHeap))))
		{
			assert(0 && "RtvDescriptorHeap : CreateHeap failed");

			return {};
		}

		DescriptorHandleIncrement = g_render.DxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		D3D12_CPU_DESCRIPTOR_HANDLE handle = heap.DxHeap->GetCPUDescriptorHandleForHeapStart();

		D3D12_RENDER_TARGET_VIEW_DESC nullDesc = {};
		nullDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		nullDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

		for(const RTVDescriptor& descriptor : g_RtvDescriptors)
		{
			g_render.DxDevice->CreateRenderTargetView(descriptor.Resource, descriptor.Resource ? &descriptor.RtvDesc : &nullDesc, handle);

			handle.ptr += DescriptorHandleIncrement;
		}

		return heap;
	}
};

struct DsvDescriptorHeap : public DescriptorHeaps
{
	virtual Dx12DescriptorHeap CreateHeap() override
	{
		Dx12DescriptorHeap heap = {};

		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		desc.NumDescriptors = (UINT)g_DsvDescriptors.Size();
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		desc.NodeMask = 0;

		if (!DXENSURE(g_render.DxDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap.DxHeap))))
		{
			assert(0 && "DsvDescriptorHeap : CreateHeap failed");

			return {};
		}

		DescriptorHandleIncrement = g_render.DxDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

		D3D12_CPU_DESCRIPTOR_HANDLE handle = heap.DxHeap->GetCPUDescriptorHandleForHeapStart();

		D3D12_DEPTH_STENCIL_VIEW_DESC nullDesc = {};
		nullDesc.Format = DXGI_FORMAT_D16_UNORM;
		nullDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

		for(const DSVDescriptor& descriptor : g_DsvDescriptors)
		{
			g_render.DxDevice->CreateDepthStencilView(descriptor.Resource, descriptor.Resource ? &descriptor.DsvDesc : &nullDesc, handle);

			handle.ptr += DescriptorHandleIncrement;
		}

		return heap;
	}
};

SrvUavDescriptorHeaps g_SrvUavHeap;
RtvDescriptorHeap g_RtvHeap;
DsvDescriptorHeap g_DsvHeap;

SparseArray<SRVUAV_t, ShaderResourceView_t> g_SrvDescriptorRemap;
SparseArray<SRVUAV_t, UnorderedAccessView_t> g_UavDescriptorRemap;
std::shared_mutex g_SrvUavRemapMutex;

bool CreateTextureSRVImpl(ShaderResourceView_t srv, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t mipLevels, uint32_t depthOrArraySize)
{
	SRVUAVDescriptor descriptor = {};

	descriptor.Type = DescriptorType::SRV;
	descriptor.Resource = Dx12_GetTextureResource(tex);

	descriptor.SrvDesc.Format = Dx12_Format(format);	
	descriptor.SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	if (dim == TextureDimension::TEX1D)
	{
		descriptor.SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1D;
		descriptor.SrvDesc.Texture1D.MostDetailedMip = 0u;
		descriptor.SrvDesc.Texture1D.MipLevels = mipLevels;
		descriptor.SrvDesc.Texture1D.ResourceMinLODClamp = 0.0f;
	}
	else if (dim == TextureDimension::TEX1D_ARRAY)
	{
		descriptor.SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
		descriptor.SrvDesc.Texture1DArray.MostDetailedMip = 0u;
		descriptor.SrvDesc.Texture1DArray.MipLevels = mipLevels;
		descriptor.SrvDesc.Texture1DArray.ArraySize = depthOrArraySize;
		descriptor.SrvDesc.Texture1DArray.ResourceMinLODClamp = 0.0f;
	}
	else if (dim == TextureDimension::TEX2D)
	{
		descriptor.SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
		descriptor.SrvDesc.Texture2D.MostDetailedMip = 0u;
		descriptor.SrvDesc.Texture2D.MipLevels = mipLevels;
		descriptor.SrvDesc.Texture2D.PlaneSlice = 0u;
		descriptor.SrvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	}
	else if (dim == TextureDimension::TEX2D_ARRAY)
	{
		descriptor.SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		descriptor.SrvDesc.Texture2DArray.MostDetailedMip = 0u;
		descriptor.SrvDesc.Texture2DArray.MipLevels = mipLevels;
		descriptor.SrvDesc.Texture2DArray.FirstArraySlice = 0u;
		descriptor.SrvDesc.Texture2DArray.ArraySize = depthOrArraySize;
		descriptor.SrvDesc.Texture2DArray.PlaneSlice = 0u;
		descriptor.SrvDesc.Texture2DArray.ResourceMinLODClamp = 0.0f;
	}
	else if (dim == TextureDimension::CUBEMAP)
	{
		descriptor.SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
		descriptor.SrvDesc.TextureCube.MostDetailedMip = 0u;
		descriptor.SrvDesc.TextureCube.MipLevels = mipLevels;
		descriptor.SrvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	}
	else if (dim == TextureDimension::CUBEMAP_ARRAY)
	{
		descriptor.SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
		descriptor.SrvDesc.TextureCubeArray.MostDetailedMip = 0u;
		descriptor.SrvDesc.TextureCubeArray.MipLevels = mipLevels;
		descriptor.SrvDesc.TextureCubeArray.First2DArrayFace = 0u;
		descriptor.SrvDesc.TextureCubeArray.NumCubes = depthOrArraySize;
		descriptor.SrvDesc.TextureCubeArray.ResourceMinLODClamp = 0.0f;
	}
	else if (dim == TextureDimension::TEX3D)
	{
		descriptor.SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
		descriptor.SrvDesc.Texture3D.MostDetailedMip = 0u;
		descriptor.SrvDesc.Texture3D.MipLevels = mipLevels;
		descriptor.SrvDesc.Texture3D.ResourceMinLODClamp = 0.0f;
	}
	else
	{
		return false;
	}

	SRVUAV_t heapHandle = g_SrvUavDescriptors.Create(descriptor);

	{
		auto lock = std::unique_lock(g_SrvUavRemapMutex);

		g_SrvDescriptorRemap.AllocCopy(srv, heapHandle);
	}	

	g_SrvUavHeap.HeapChanged();

	return true;
}

bool CreateTextureUAVImpl(UnorderedAccessView_t uav, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize, uint32_t mipSlice)
{
	SRVUAVDescriptor descriptor = {};

	descriptor.Type = DescriptorType::UAV;
	descriptor.Resource = Dx12_GetTextureResource(tex);
	descriptor.UavDesc.Format = Dx12_Format(format);
	
	if (dim == TextureDimension::TEX1D)
	{
		descriptor.UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
		descriptor.UavDesc.Texture1D.MipSlice = mipSlice;
	}
	else if (dim == TextureDimension::TEX1D_ARRAY)
	{
		descriptor.UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
		descriptor.UavDesc.Texture1DArray.MipSlice = mipSlice;
		descriptor.UavDesc.Texture1DArray.FirstArraySlice = 0u;
		descriptor.UavDesc.Texture1DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::TEX2D)
	{
		descriptor.UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
		descriptor.UavDesc.Texture2D.MipSlice = mipSlice;
		descriptor.UavDesc.Texture2D.PlaneSlice = 0u;
	}
	else if (dim == TextureDimension::TEX2D_ARRAY)
	{
		descriptor.UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		descriptor.UavDesc.Texture2DArray.MipSlice = mipSlice;
		descriptor.UavDesc.Texture2DArray.FirstArraySlice = 0u;
		descriptor.UavDesc.Texture2DArray.ArraySize = depthOrArraySize;
		descriptor.UavDesc.Texture2DArray.PlaneSlice = 0u;
	}
	else if (dim == TextureDimension::CUBEMAP)
	{
		descriptor.UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		descriptor.UavDesc.Texture2DArray.MipSlice = mipSlice;
		descriptor.UavDesc.Texture2DArray.FirstArraySlice = 0u;
		descriptor.UavDesc.Texture2DArray.ArraySize = 6u;
		descriptor.UavDesc.Texture2DArray.PlaneSlice = 0u;
	}
	else if (dim == TextureDimension::CUBEMAP_ARRAY)
	{
		descriptor.UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		descriptor.UavDesc.Texture2DArray.MipSlice = mipSlice;
		descriptor.UavDesc.Texture2DArray.FirstArraySlice = 0u;
		descriptor.UavDesc.Texture2DArray.ArraySize = 6u * depthOrArraySize;
		descriptor.UavDesc.Texture2DArray.PlaneSlice = 0u;
	}
	else if (dim == TextureDimension::TEX3D)
	{
		descriptor.UavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
		descriptor.UavDesc.Texture3D.MipSlice = mipSlice;
		descriptor.UavDesc.Texture3D.FirstWSlice = 0u;
		descriptor.UavDesc.Texture3D.WSize = -1;
	}
	else
	{
		return false;
	}

	SRVUAV_t heapHandle = g_SrvUavDescriptors.Create(descriptor);

	{
		auto lock = std::unique_lock(g_SrvUavRemapMutex);

		g_UavDescriptorRemap.AllocCopy(uav, heapHandle);
	}	

	g_SrvUavHeap.HeapChanged();

	return true;
}

bool CreateTextureRTVImpl(RenderTargetView_t rtv, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize)
{
	RTVDescriptor descriptor = {};

	descriptor.Resource = Dx12_GetTextureResource(tex);

	descriptor.RtvDesc.Format = Dx12_Format(format);

	if (dim == TextureDimension::TEX1D)
	{
		descriptor.RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1D;
		descriptor.RtvDesc.Texture1D.MipSlice = 0u;
	}
	else if (dim == TextureDimension::TEX1D_ARRAY)
	{
		descriptor.RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE1DARRAY;
		descriptor.RtvDesc.Texture1DArray.MipSlice = 0u;
		descriptor.RtvDesc.Texture1DArray.FirstArraySlice = 0u;
		descriptor.RtvDesc.Texture1DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::TEX2D)
	{
		descriptor.RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		descriptor.RtvDesc.Texture2D.MipSlice = 0u;
		descriptor.RtvDesc.Texture2D.PlaneSlice = 0u;
	}
	else if (dim == TextureDimension::TEX2D_ARRAY)
	{
		descriptor.RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		descriptor.RtvDesc.Texture2DArray.MipSlice = 0u;
		descriptor.RtvDesc.Texture2DArray.FirstArraySlice = 0u;
		descriptor.RtvDesc.Texture2DArray.ArraySize = depthOrArraySize;
		descriptor.RtvDesc.Texture2DArray.PlaneSlice = 0u;
	}
	else if (dim == TextureDimension::CUBEMAP)
	{
		descriptor.RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		descriptor.RtvDesc.Texture2DArray.MipSlice = 0u;
		descriptor.RtvDesc.Texture2DArray.FirstArraySlice = 0u;
		descriptor.RtvDesc.Texture2DArray.ArraySize = 6u;
		descriptor.RtvDesc.Texture2DArray.PlaneSlice = 0u;
	}
	else if (dim == TextureDimension::CUBEMAP_ARRAY)
	{
		descriptor.RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
		descriptor.RtvDesc.Texture2DArray.MipSlice = 0u;
		descriptor.RtvDesc.Texture2DArray.FirstArraySlice = 0u;
		descriptor.RtvDesc.Texture2DArray.ArraySize = 6u * depthOrArraySize;
		descriptor.RtvDesc.Texture2DArray.PlaneSlice = 0u;
	}
	else if (dim == TextureDimension::TEX3D)
	{
		descriptor.RtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE3D;
		descriptor.RtvDesc.Texture3D.MipSlice = 0u;
		descriptor.RtvDesc.Texture3D.FirstWSlice = 0u;
		descriptor.RtvDesc.Texture3D.WSize = -1;
	}
	else
	{
		return false;
	}

	{
		auto lock = std::unique_lock(g_RtvMutex);

		g_RtvDescriptors.AllocCopy(rtv, descriptor);
	}	

	g_RtvHeap.HeapChanged();

	return true;
}

bool CreateTextureDSVImpl(DepthStencilView_t dsv, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize)
{
	DSVDescriptor descriptor = {};

	descriptor.Resource = Dx12_GetTextureResource(tex);

	descriptor.DsvDesc.Format = Dx12_Format(format);

	if (dim == TextureDimension::TEX1D)
	{
		descriptor.DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1D;
		descriptor.DsvDesc.Texture1D.MipSlice = 0u;
	}
	else if (dim == TextureDimension::TEX1D_ARRAY)
	{
		descriptor.DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE1DARRAY;
		descriptor.DsvDesc.Texture1DArray.MipSlice = 0u;
		descriptor.DsvDesc.Texture1DArray.FirstArraySlice = 0u;
		descriptor.DsvDesc.Texture1DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::TEX2D)
	{
		descriptor.DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
		descriptor.DsvDesc.Texture2D.MipSlice = 0u;
	}
	else if (dim == TextureDimension::TEX2D_ARRAY)
	{
		descriptor.DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		descriptor.DsvDesc.Texture2DArray.MipSlice = 0u;
		descriptor.DsvDesc.Texture2DArray.FirstArraySlice = 0u;
		descriptor.DsvDesc.Texture2DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::CUBEMAP)
	{
		descriptor.DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		descriptor.DsvDesc.Texture2DArray.MipSlice = 0u;
		descriptor.DsvDesc.Texture2DArray.FirstArraySlice = 0u;
		descriptor.DsvDesc.Texture2DArray.ArraySize = 6u;
	}
	else if (dim == TextureDimension::CUBEMAP_ARRAY)
	{
		descriptor.DsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
		descriptor.DsvDesc.Texture2DArray.MipSlice = 0u;
		descriptor.DsvDesc.Texture2DArray.FirstArraySlice = 0u;
		descriptor.DsvDesc.Texture2DArray.ArraySize = 6u * depthOrArraySize;
	}
	else
	{
		return false;
	}

	{
		auto lock = std::unique_lock(g_DsvMutex);

		g_DsvDescriptors.AllocCopy(dsv, descriptor);
	}
	

	g_DsvHeap.HeapChanged();

	return true;
}

void BindTextureSrvUavImpl(SRVUAV_t handle, Texture_t tex)
{
	{
		auto lock = g_SrvUavDescriptors.ReadScopeLock();

		SRVUAVDescriptor* descriptor = g_SrvUavDescriptors.Get(handle);

		descriptor->Resource = Dx12_GetTextureResource(tex);
	}

	g_SrvUavHeap.HeapChanged();
}

void BindTextureSRVImpl(ShaderResourceView_t srv, Texture_t tex)
{
	auto lock = std::shared_lock(g_SrvUavRemapMutex);

	BindTextureSrvUavImpl(g_SrvDescriptorRemap[srv], tex);
}

void BindTextureUAVImpl(UnorderedAccessView_t uav, Texture_t tex)
{
	auto lock = std::shared_lock(g_SrvUavRemapMutex);

	BindTextureSrvUavImpl(g_UavDescriptorRemap[uav], tex);
}

void BindTextureRTVImpl(RenderTargetView_t rtv, Texture_t tex)
{
	{
		auto lock = std::shared_lock(g_RtvMutex);

		RTVDescriptor& descriptor = g_RtvDescriptors[rtv];

		descriptor.Resource = Dx12_GetTextureResource(tex);
	}

	g_RtvHeap.HeapChanged();
}

void BindTextureDSVImpl(DepthStencilView_t dsv, Texture_t tex)
{
	{
		auto lock = std::shared_lock(g_DsvMutex);

		DSVDescriptor& descriptor = g_DsvDescriptors[dsv];

		descriptor.Resource = Dx12_GetTextureResource(tex);
	}

	g_DsvHeap.HeapChanged();
}

bool CreateStructuredBufferSRVImpl(ShaderResourceView_t srv, StructuredBuffer_t buf, uint32_t firstElement, uint32_t numElements, uint32_t structureByteStride)
{
	SRVUAVDescriptor descriptor = {};

	uint32_t bufferOffset = Dx12_GetBufferOffset(buf);
	assert(bufferOffset % structureByteStride == 0);
	uint32_t sharedBufferFirstElem = bufferOffset / structureByteStride;

	descriptor.Type = DescriptorType::SRV;
	descriptor.Resource = Dx12_GetBufferResource(buf);
	descriptor.SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	descriptor.SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	descriptor.SrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	descriptor.SrvDesc.Buffer.FirstElement = sharedBufferFirstElem + firstElement;
	descriptor.SrvDesc.Buffer.NumElements = numElements;
	descriptor.SrvDesc.Buffer.StructureByteStride = structureByteStride;
	descriptor.SrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	SRVUAV_t heapHandle = g_SrvUavDescriptors.Create(descriptor);

	{
		auto lock = std::unique_lock(g_SrvUavRemapMutex);

		g_SrvDescriptorRemap.AllocCopy(srv, heapHandle);
	}

	g_SrvUavHeap.HeapChanged();

	return true;
}

bool CreateStructuredBufferUAVImpl(UnorderedAccessView_t uav, StructuredBuffer_t buf, uint32_t firstElement, uint32_t numElements, uint32_t structureByteStride)
{
	SRVUAVDescriptor descriptor = {};

	uint32_t bufferOffset = Dx12_GetBufferOffset(buf);
	assert(bufferOffset == 0); // Cannot share UAV buffers due to transitioning limitations

	descriptor.Type = DescriptorType::UAV;
	descriptor.Resource = Dx12_GetBufferResource(buf);
	descriptor.UavDesc.Format = DXGI_FORMAT_UNKNOWN;
	descriptor.UavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	descriptor.UavDesc.Buffer.FirstElement = firstElement;
	descriptor.UavDesc.Buffer.NumElements = numElements;
	descriptor.UavDesc.Buffer.StructureByteStride = structureByteStride;
	descriptor.UavDesc.Buffer.CounterOffsetInBytes = 0;
	descriptor.UavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	SRVUAV_t heapHandle = g_SrvUavDescriptors.Create(descriptor);

	{
		auto lock = std::unique_lock(g_SrvUavRemapMutex);

		g_UavDescriptorRemap.AllocCopy(uav, heapHandle);
	}

	g_SrvUavHeap.HeapChanged();

	return true;
}

void DestroySRV(ShaderResourceView_t srv)
{
	{
		auto lock = std::unique_lock(g_SrvUavRemapMutex);

		g_SrvUavDescriptors.Release(g_SrvDescriptorRemap[srv]);

		g_SrvDescriptorRemap.Free(srv);
	}

	g_SrvUavHeap.HeapChanged();
}

void DestroyUAV(UnorderedAccessView_t uav)
{
	{
		auto lock = std::unique_lock(g_SrvUavRemapMutex);

		g_SrvUavDescriptors.Release(g_UavDescriptorRemap[uav]);

		g_UavDescriptorRemap.Free(uav);
	}

	g_SrvUavHeap.HeapChanged();
}

void DestroyRTV(RenderTargetView_t rtv)
{
	{
		auto lock = std::unique_lock(g_RtvMutex);

		g_RtvDescriptors.Free(rtv);
	}	

	g_RtvHeap.HeapChanged();
}

void DestroyDSV(DepthStencilView_t dsv)
{
	{
		auto lock = std::unique_lock(g_DsvMutex);

		g_DsvDescriptors.Free(dsv);
	}	

	g_DsvHeap.HeapChanged();
}

void Dx12_DescriptorsBeginFrame()
{
	g_SrvUavHeap.BeginFrame();
	g_RtvHeap.BeginFrame();
	g_DsvHeap.BeginFrame();
}

Dx12DescriptorHeap Dx12_AccquireSrvUavHeap()
{
	return g_SrvUavHeap.AccquireHeap();
}

Dx12DescriptorHeap Dx12_AccquireRtvHeap()
{
	return g_RtvHeap.AccquireHeap();
}

Dx12DescriptorHeap Dx12_AccquireDsvHeap()
{
	return g_DsvHeap.AccquireHeap();
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12_RtvDescriptorHandle(ID3D12DescriptorHeap* heap, RenderTargetView_t rtv)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();

	handle.ptr += (UINT)rtv * g_RtvHeap.DescriptorHandleIncrement;

	return handle;
}

D3D12_CPU_DESCRIPTOR_HANDLE Dx12_DsvDescriptorHandle(ID3D12DescriptorHeap* heap, DepthStencilView_t dsv)
{
	D3D12_CPU_DESCRIPTOR_HANDLE handle = heap->GetCPUDescriptorHandleForHeapStart();

	handle.ptr += (UINT)dsv * g_DsvHeap.DescriptorHandleIncrement;

	return handle;
}

D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetSrvAddress(ID3D12DescriptorHeap* heap, ShaderResourceView_t srv)
{
	D3D12_GPU_VIRTUAL_ADDRESS ptr = heap->GetGPUDescriptorHandleForHeapStart().ptr;
	ptr += (UINT)srv * g_SrvUavHeap.DescriptorHandleIncrement;

	return ptr;
}

D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetUavAddress(ID3D12DescriptorHeap* heap, UnorderedAccessView_t uav)
{
	D3D12_GPU_VIRTUAL_ADDRESS ptr = heap->GetGPUDescriptorHandleForHeapStart().ptr;
	ptr += (UINT)uav * g_SrvUavHeap.DescriptorHandleIncrement;

	return ptr;
}

D3D12_GPU_DESCRIPTOR_HANDLE Dx12_GetSrvUavTableHandle(ID3D12DescriptorHeap* heap)
{
	return heap->GetGPUDescriptorHandleForHeapStart();
}

void Dx12_SubmitSrvUavDescriptorHeap(Dx12DescriptorHeap&& heap, uint64_t graphicsFenceValue, uint64_t computeFenceValue)
{
	g_SrvUavHeap.SubmitHeap(std::move(heap), graphicsFenceValue, computeFenceValue);
}

void Dx12_SubmitRtvDescriptorHeap(Dx12DescriptorHeap&& heap, uint64_t graphicsFenceValue, uint64_t computeFenceValue)
{
	g_RtvHeap.SubmitHeap(std::move(heap), graphicsFenceValue, computeFenceValue);
}

void Dx12_SubmitDsvDescriptorHeap(Dx12DescriptorHeap&& heap, uint64_t graphicsFenceValue, uint64_t computeFenceValue)
{
	g_DsvHeap.SubmitHeap(std::move(heap), graphicsFenceValue, computeFenceValue);
}

uint32_t GetDescriptorIndexImpl(ShaderResourceView_t srv)
{
	auto lock = std::shared_lock(g_SrvUavRemapMutex);

	return static_cast<uint32_t>(g_SrvDescriptorRemap[srv]);
}

uint32_t GetDescriptorIndexImpl(UnorderedAccessView_t uav)
{
	auto lock = std::shared_lock(g_SrvUavRemapMutex);

	return static_cast<uint32_t>(g_UavDescriptorRemap[uav]);
}

}
