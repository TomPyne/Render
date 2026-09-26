#include "Impl/BuffersImpl.h"

#include "RenderImpl.h"

namespace rl
{

static const size_t FrameConstantPageSize = 4u * 1024u * 1024u;
static const size_t DedicatedPageAlignment = 64u * 1024u;

template<typename T>
static constexpr T AlignUp(T size, T alignment)
{
	const T mask = alignment - 1;
	return (size + mask) & ~mask;
}

struct FrameConstantPage
{
	ComPtr<ID3D12Resource> DxRes;
	size_t Size = 0u;
	uint64_t GraphicsFrameFence = 0u;
	uint64_t ComputeFrameFence = 0u;

	bool IsDedicated() const { return Size != FrameConstantPageSize; }

	bool InFlight(uint64_t completedGraphicsFence, uint64_t completedComputeFence) const
	{
		return completedGraphicsFence < GraphicsFrameFence || completedComputeFence < ComputeFrameFence;
	}
};

std::vector<FrameConstantPage> g_ActiveFrameConstantPages;
std::vector<FrameConstantPage> g_InFlightFrameConstantPages;
std::vector<FrameConstantPage> g_AvailableFrameConstantPages;

static ID3D12Resource* AcquireFrameConstantPage(size_t size)
{
	FrameConstantPage page;

	if (size <= FrameConstantPageSize && !g_AvailableFrameConstantPages.empty())
	{
		page = std::move(g_AvailableFrameConstantPages.back());
		g_AvailableFrameConstantPages.pop_back();
	}
	else
	{
		page.Size = size <= FrameConstantPageSize ? FrameConstantPageSize : AlignUp(size, DedicatedPageAlignment);
		page.DxRes = Dx12_CreateBuffer(page.Size, D3D12_HEAP_TYPE_DEFAULT, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_FLAG_NONE);
		page.DxRes->SetName(page.IsDedicated() ? L"FrameConstantPage (Dedicated)" : L"FrameConstantPage");
	}

	g_ActiveFrameConstantPages.emplace_back(std::move(page));

	return g_ActiveFrameConstantPages.back().DxRes.Get();
}

GPUAddress_t UploadFrameConstants(CommandList* cl, const ConstantUploadSpan_s* spans, size_t numSpans, size_t totalSize)
{
	assert(cl && "UploadFrameConstants requires a command list");
	assert(spans && numSpans > 0u && totalSize > 0u && "UploadFrameConstants called with nothing to upload");

	ID3D12Resource* dstRes = AcquireFrameConstantPage(totalSize);
	ID3D12GraphicsCommandList* dxCl = Dx12_GetCommandList(cl);

	for (size_t i = 0; i < numSpans; ++i)
	{
		const ConstantUploadSpan_s& span = spans[i];

		assert((span.DstOffset % D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) == 0u && "UploadFrameConstants span DstOffset must be 256 byte aligned");
		assert(span.DstOffset + span.Size <= totalSize && "UploadFrameConstants span exceeds totalSize");

		if (span.Size == 0u)
			continue;

		const Dx12UploadAllocation staging = Dx12_AllocateDynamicUpload(span.Size, D3D12_RAW_UAV_SRV_BYTE_ALIGNMENT);

		memcpy(staging.CpuMem, span.Data, span.Size);

		dxCl->CopyBufferRegion(dstRes, span.DstOffset, staging.Resource, staging.Offset, span.Size);
	}

	D3D12_RESOURCE_BARRIER barrier = Dx12_TransitionBarrier(dstRes, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
	dxCl->ResourceBarrier(1u, &barrier);

	return static_cast<GPUAddress_t>(dstRes->GetGPUVirtualAddress());
}

size_t GetFrameConstantPageCount()
{
	return g_ActiveFrameConstantPages.size() + g_InFlightFrameConstantPages.size() + g_AvailableFrameConstantPages.size();
}

void Dx12_FrameConstantsNewFrame()
{
	const uint64_t completedGraphicsFence = g_render.DirectQueue.DxFence->GetCompletedValue();
	const uint64_t completedComputeFence = g_render.ComputeQueue.DxFence->GetCompletedValue();

	for (int32_t i = (int32_t)g_InFlightFrameConstantPages.size() - 1; i >= 0; --i)
	{
		FrameConstantPage& page = g_InFlightFrameConstantPages[i];

		if (page.InFlight(completedGraphicsFence, completedComputeFence))
			continue;

		// Dedicated pages are sized for a single spike, release them rather than holding onto the memory
		if (!page.IsDedicated())
		{
			g_AvailableFrameConstantPages.emplace_back(std::move(page));
		}

		g_InFlightFrameConstantPages.erase(g_InFlightFrameConstantPages.begin() + i);
	}
}

void Dx12_FrameConstantsEndFrame(uint64_t graphicsFrameFence, uint64_t computeFrameFence)
{
	for (FrameConstantPage& page : g_ActiveFrameConstantPages)
	{
		page.GraphicsFrameFence = graphicsFrameFence;
		page.ComputeFrameFence = computeFrameFence;

		g_InFlightFrameConstantPages.emplace_back(std::move(page));
	}

	g_ActiveFrameConstantPages.clear();
}

}
