#include "View.h"

#include "RenderImpl.h"

#include "Binding.h"
#include "CommandList.h"

#include <dxgi1_3.h>
#include <map>

#ifdef _MSC_VER
#pragma comment(lib, "dxgi.lib")
#endif

namespace rl
{

std::map<intptr_t, RenderView*> g_views;

struct RenderViewImpl
{
	ComPtr<IDXGISwapChain1> DxSwapChain;
	Texture_t Textures[RenderView::NumBackBuffers];
	RenderTargetView_t Rtv[RenderView::NumBackBuffers];
	uint64_t FrameFenceValue[RenderView::NumBackBuffers];
	uint64_t FrameId = 0;

	ComPtr<ID3D12Fence> DxFrameFence;
};

RenderView::RenderView()
	: Impl(std::make_unique<RenderViewImpl>())
{
}

RenderView::~RenderView()
{
	for (uint32_t i = 0; i < RenderView::NumBackBuffers; i++)
	{
		RenderRelease(Impl->Rtv[i]);
		RenderRelease(Impl->Textures[i]);
	}
	
	g_views.erase(Hwnd);
}

void RenderView::Sync()
{
	uint64_t fenceVal = Impl->FrameFenceValue[Impl->FrameId % RenderView::NumBackBuffers];
	DXENSURE(Impl->DxFrameFence->SetEventOnCompletion(fenceVal, nullptr));
}

void RenderView::Resize(uint32_t x, uint32_t y)
{
	x = x > 0 ? x : 1;
	y = y > 0 ? y : 1;

	if (Width == x && Height == y)
		return;

	Width = x;
	Height = y;

	for (uint32_t i = 0; i < RenderView::NumBackBuffers; i++)
	{
		RenderRelease(Impl->Rtv[i]);
		RenderRelease(Impl->Textures[i]);

		Impl->Rtv[i] = RenderTargetView_t::INVALID;
		Impl->Textures[i] = Texture_t::INVALID;
	}

	Dx12_TexturesProcessPendingDeletes(true);

	for (uint32_t i = 0; i < RenderView::NumBackBuffers; i++)
	{
		if(Impl->FrameFenceValue[i] > 0)
			DXENSURE(Impl->DxFrameFence->SetEventOnCompletion(Impl->FrameFenceValue[i], nullptr));
	}

	DXENSURE(Impl->DxSwapChain->ResizeBuffers(0, (UINT)Width, (UINT)Height, DXGI_FORMAT_UNKNOWN, 0));

	for (uint32_t i = 0; i < RenderView::NumBackBuffers; i++)
	{
		ComPtr<ID3D12Resource> backBuffer = nullptr;

		Impl->DxSwapChain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));

		backBuffer->SetName(L"BackbufferTexture");

		Impl->Textures[i] = AllocTexture();

		Dx12_SetTextureResource(Impl->Textures[i], backBuffer);		

		Impl->Rtv[i] = CreateTextureRTV(Impl->Textures[i], BackBufferFormat, TextureDimension::TEX2D, 1);
	}

	// After recreating the swap chain, synchronise our frame ID back to slot 0.
	// Otherwise starting on target 1 throws an error.
	while (Impl->FrameId % RenderView::NumBackBuffers != 0)
	{
		Impl->FrameId++;
	}
}

void RenderView::Present(bool vsync, bool sync)
{
	Impl->DxSwapChain->Present(vsync, 0);

	Impl->FrameFenceValue[Impl->FrameId % RenderView::NumBackBuffers] = ++Impl->FrameId;

	g_render.DirectQueue.DxCommandQueue->Signal(Impl->DxFrameFence.Get(), Impl->FrameId);

	if (sync)
	{
		Sync();
	}		
}

RenderViewPtr CreateRenderViewPtr(intptr_t hwnd)
{
	return std::shared_ptr<RenderView>(CreateRenderView(hwnd));
}

RenderView* CreateRenderView(intptr_t hwnd)
{
	RenderView* rv = new RenderView;

	rv->Hwnd = hwnd;

	DXGI_SWAP_CHAIN_DESC1 sd;
	ZeroMemory(&sd, sizeof(sd));

	sd.Width = 0;
	sd.Height = 0;
	sd.Format = Dx12_Format(RenderView::BackBufferFormat);
	sd.Stereo = FALSE;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount = RenderView::NumBackBuffers;
	sd.Scaling = DXGI_SCALING_STRETCH;
	sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	sd.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	UINT factoryFlags = g_render.Debug ? DXGI_CREATE_FACTORY_DEBUG : 0u;

	ComPtr<IDXGIFactory2> factory;
	if (!DXENSURE(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&factory))))
		return nullptr;

	if (!DXENSURE(factory->CreateSwapChainForHwnd(g_render.DirectQueue.DxCommandQueue.Get(), (HWND)hwnd, &sd, NULL, NULL, &rv->Impl->DxSwapChain)))
		return nullptr;

	if (!DXENSURE(g_render.DxDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&rv->Impl->DxFrameFence))))
		return nullptr;

	g_views[hwnd] = rv;

	return rv;
}

RenderView* GetRenderViewForHwnd(intptr_t hwnd)
{
	auto iter = g_views.find(hwnd);
	return iter != g_views.end() ? iter->second : nullptr;
}

void RenderView::ClearCurrentBackBufferTarget(CommandList* cl)
{
	constexpr float DefaultClearCol[4] = { 0.0f, 0.0f, 0.2f, 0.0f };
	ClearCurrentBackBufferTarget( cl, DefaultClearCol );
}

void RenderView::ClearCurrentBackBufferTarget(CommandList* cl, const float clearCol[4])
{
	RenderTargetView_t rtv = GetCurrentBackBufferRTV();
	if (rtv != RenderTargetView_t::INVALID)
	{
		cl->ClearRenderTarget(rtv, clearCol);
	}
}

Texture_t RenderView::GetCurrentBackBufferTexture() const
{
	return Impl->Textures[Impl->FrameId % RenderView::NumBackBuffers];
}

RenderTargetView_t RenderView::GetCurrentBackBufferRTV() const
{
	return Impl->Rtv[Impl->FrameId % RenderView::NumBackBuffers];
}

uint64_t RenderView::GetFrameID() const
{
	return Impl->FrameId;
}

}
