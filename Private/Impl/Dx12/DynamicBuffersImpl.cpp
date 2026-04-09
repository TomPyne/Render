#include "Impl/BuffersImpl.h"

#include "RenderImpl.h"

#include <deque>

namespace rl
{

static const size_t AllocationPageSize = 2u * 1024u * 1024u;

template<typename T>
static constexpr T AlignUp(T size, T alignment)
{
	const T mask = alignment - 1;
	return (size + mask) & ~mask;
}

struct DynamicAllocation
{
	void* pCpuMem = nullptr;
	D3D12_GPU_VIRTUAL_ADDRESS pGpuMem = (D3D12_GPU_VIRTUAL_ADDRESS)0;
	uint32_t Size = 0u;
	ID3D12Resource* DxResource = nullptr;
};

struct DynamicUploadBuffer
{
	explicit DynamicUploadBuffer() {}

	static const size_t GetPageSize() { return AllocationPageSize; }

	DynamicAllocation Allocate(size_t sizeInBytes, size_t alignment)
	{
		assert(sizeInBytes < AllocationPageSize && "DynamicUploadBuffer::AllocateBuffer allocation exceeds max page size for dynamic buffers");

		if (!CurrentPage || !CurrentPage->HasSpace(sizeInBytes, alignment))
		{
			CurrentPage = RequestPage();
		}

		return CurrentPage->Allocate(sizeInBytes, alignment);
	}

	DynamicAllocation AllocateBuffer(size_t sizeInBytes)
	{
		return Allocate(sizeInBytes, D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT);
	}

	DynamicAllocation AllocateConstantBuffer(size_t sizeInBytes)
	{
		return Allocate(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
	}

	void Reset(uint64_t graphicsFrameFence, uint64_t ComputeFrameFence)
	{
		CurrentPage = nullptr;
		AvailablePages = Pages;
		GraphicsFrameFence = graphicsFrameFence;
		ComputeFrameFence = ComputeFrameFence;

		for (auto& page : AvailablePages)
		{
			page->Reset();
		}
	}

	bool InFlight(uint64_t graphicsFrameFence, uint64_t computeFrameFence) const
	{
		return graphicsFrameFence <= GraphicsFrameFence || computeFrameFence <= ComputeFrameFence;
	}

private:

	struct AllocationPage
	{
		AllocationPage()
		{
			DxRes = Dx12_CreateBuffer(AllocationPageSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_FLAG_NONE);

			pGpuMem = DxRes->GetGPUVirtualAddress();
			DxRes->Map(0, nullptr, &pCpuMem);
		}

		~AllocationPage()
		{
			DxRes->Unmap(0, nullptr);
		}

		bool HasSpace(size_t sizeInBytes, size_t alignment) const
		{
			size_t alignedSize = AlignUp(sizeInBytes, alignment);
			size_t alignedOffset = AlignUp(Offset, alignment);

			return (alignedOffset + alignedSize) <= AllocationPageSize;
		}

		DynamicAllocation Allocate(size_t sizeInBytes, size_t alignment)
		{
			size_t alignedSize = AlignUp(sizeInBytes, alignment);
			Offset = AlignUp(Offset, alignment);

			DynamicAllocation alloc;
			alloc.pCpuMem = static_cast<uint8_t*>(pCpuMem) + Offset;
			alloc.pGpuMem = pGpuMem + Offset;
			alloc.Size = (uint32_t)alignedSize;
			alloc.DxResource = DxRes.Get();

			Offset += alignedSize;

			return alloc;
		}

		void Reset()
		{
			Offset = 0;
		}

	private:
		ComPtr<ID3D12Resource> DxRes;

		void* pCpuMem = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS pGpuMem = (D3D12_GPU_VIRTUAL_ADDRESS)0;

		size_t Offset = 0;
	};

	using PagePool = std::deque<std::shared_ptr<AllocationPage>>;

	AllocationPage* RequestPage()
	{
		AllocationPage* page = nullptr;

		if (AvailablePages.empty())
		{
			Pages.emplace_back(std::make_shared<AllocationPage>());
			page = Pages.back().get();
		}
		else
		{
			page = AvailablePages.front().get();
			AvailablePages.pop_front();
		}

		return page;
	}

	PagePool Pages;
	PagePool AvailablePages;

	AllocationPage* CurrentPage = nullptr;

	uint64_t GraphicsFrameFence = 0u;
	uint64_t ComputeFrameFence = 0u;
};

DynamicUploadBuffer* g_dynamicUploadBuffer = nullptr;
std::vector<DynamicAllocation> g_dynamicBuffers;

std::vector<DynamicUploadBuffer> g_ActiveDynamicBuffers;
std::vector<DynamicUploadBuffer> g_InFlightDynamicBuffers;
std::vector<DynamicUploadBuffer> g_AvailableDynamicBuffers;

DynamicBuffer_t CreateDynamicBuffer(const void* const data, size_t size, bool isConstantBuf)
{
	assert(g_dynamicUploadBuffer && "g_dynamicUploadBuffer null, DynamicBuffers_NewFrame not called");

	auto alloc = isConstantBuf ? g_dynamicUploadBuffer->AllocateConstantBuffer(size) : g_dynamicUploadBuffer->AllocateBuffer(size);

	memcpy(alloc.pCpuMem, data, size);

	g_dynamicBuffers.push_back(alloc);

	return (DynamicBuffer_t)(g_dynamicBuffers.size() - 1u);
}

DynamicBuffer_t CreateDynamicVertexBuffer(const void* const data, size_t size)
{
	return CreateDynamicBuffer(data, size, false);
}

DynamicBuffer_t CreateDynamicIndexBuffer(const void* const data, size_t size)
{
	return CreateDynamicBuffer(data, size, false);
}

DynamicBuffer_t CreateDynamicConstantBuffer(const void* const data, size_t size)
{
	return CreateDynamicBuffer(data, size, true);
}

DynamicBuffer_t CreateDynamicByteBuffer(const void* const data, size_t size)
{
	return CreateDynamicBuffer(data, size, false);
}

void DynamicBuffers_NewFrame()
{
	assert(g_dynamicUploadBuffer == nullptr && "DynamicBuffers_NewFrame called without DynamicBuffers_EndFrame");

	g_dynamicBuffers.clear();
	g_dynamicBuffers.push_back({});

	const uint64_t graphicsFrameFence = g_render.DirectQueue.DxFence->GetCompletedValue();
	const uint64_t computeFrameFence = g_render.ComputeQueue.DxFence->GetCompletedValue();

	for (int32_t i = (int32_t)g_InFlightDynamicBuffers.size() - 1; i >= 0; --i)
	{
		if (!g_InFlightDynamicBuffers[i].InFlight(graphicsFrameFence, computeFrameFence))
		{
			g_AvailableDynamicBuffers.emplace_back(std::move(g_InFlightDynamicBuffers[i]));
			g_InFlightDynamicBuffers.erase(g_InFlightDynamicBuffers.begin() + i);
		}
	}

	if (g_AvailableDynamicBuffers.empty())
	{
		g_AvailableDynamicBuffers.resize(1);
	}

	g_ActiveDynamicBuffers.emplace_back(std::move(g_AvailableDynamicBuffers.back()));

	g_AvailableDynamicBuffers.pop_back();

	g_dynamicUploadBuffer = &g_ActiveDynamicBuffers.back();
}

void DynamicBuffers_EndFrame()
{
	const uint64_t graphicsFrameFence = Dx12_Signal(CommandListType::GRAPHICS);
	const uint64_t computeFrameFence = Dx12_Signal(CommandListType::COMPUTE);

	for (DynamicUploadBuffer& buf : g_ActiveDynamicBuffers)
	{
		buf.Reset(graphicsFrameFence, computeFrameFence);

		g_InFlightDynamicBuffers.emplace_back(std::move(buf));
	}

	g_ActiveDynamicBuffers.clear();

	g_dynamicUploadBuffer = nullptr;
}

D3D12_VERTEX_BUFFER_VIEW Dx12_GetVertexBufferView(DynamicBuffer_t db, uint32_t offset, uint32_t stride)
{
	assert((size_t)db < g_dynamicBuffers.size() && "Dx12_GetVertexBufferView invalid db");

	const DynamicAllocation& alloc = g_dynamicBuffers[(size_t)db];

	D3D12_VERTEX_BUFFER_VIEW view;
	view.BufferLocation = alloc.pGpuMem + (D3D12_GPU_VIRTUAL_ADDRESS)offset;
	view.SizeInBytes = (UINT)alloc.Size;
	view.StrideInBytes = (UINT)stride;

	return view;
}

D3D12_GPU_VIRTUAL_ADDRESS Dx12_GetDbAddress(DynamicBuffer_t db)
{
	assert((size_t)db < g_dynamicBuffers.size() && "Dx12_GetDbAddress invalid db");

	const DynamicAllocation& alloc = g_dynamicBuffers[(size_t)db];

	return alloc.pGpuMem;
}

D3D12_INDEX_BUFFER_VIEW Dx12_GetIndexBufferView(DynamicBuffer_t db, RenderFormat format, uint32_t offset)
{
	assert((size_t)db < g_dynamicBuffers.size() && "Dx12_GetIndexBufferView invalid db");

	const DynamicAllocation& alloc = g_dynamicBuffers[(size_t)db];

	D3D12_INDEX_BUFFER_VIEW view;
	view.BufferLocation = alloc.pGpuMem + (D3D12_GPU_VIRTUAL_ADDRESS)offset;
	view.SizeInBytes = (UINT)alloc.Size;
	view.Format = Dx12_Format(format);

	return view;
}

ID3D12Resource* Dx12_GetDynamicBufferResource(DynamicBuffer_t db, size_t* outOffset)
{
	assert((size_t)db < g_dynamicBuffers.size() && "Dx12_GetDynamicBufferResource invalid db");

	const DynamicAllocation& alloc = g_dynamicBuffers[(size_t)db];

	if (outOffset && alloc.DxResource)
	{
		*outOffset = alloc.pGpuMem - alloc.DxResource->GetGPUVirtualAddress();
	}

	return alloc.DxResource;
}

}