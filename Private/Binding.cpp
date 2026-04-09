#include "Binding.h"
#include "Textures.h"
#include "Impl/BindingImpl.h"
#include "IDArray.h"

#include <mutex>

namespace rl
{

enum class ViewResourceType : uint8_t
{
	Unknown = 0,
	Texture,
	StructuredBuffer
};

struct TextureViewData
{
	Texture_t Handle;
	RenderFormat Format;
	uint32_t MipLevelsOrSlice;
	uint32_t DepthOrArraySize;
	TextureViewData(Texture_t inTexture, RenderFormat inFormat, uint32_t inDepthOrArraySize = 1, uint32_t mipLevelsOrSlice = uint32_t(-1))
		: Handle(inTexture)
		, Format(inFormat)
		, MipLevelsOrSlice(mipLevelsOrSlice)
		, DepthOrArraySize(inDepthOrArraySize)
	{}
};

struct BufferViewData
{
	StructuredBuffer_t Handle;
	uint32_t FirstElem;
	uint32_t NumElems;
	uint32_t Stride;
	BufferViewData(StructuredBuffer_t inBuffer, uint32_t inFirstElem, uint32_t inNumElems, uint32_t stride)
		: Handle(inBuffer)
		, FirstElem(inFirstElem)
		, NumElems(inNumElems)
		, Stride(stride)
	{}
};

struct ViewData
{
	ViewResourceType Type = ViewResourceType::Unknown;
	union
	{
		TextureViewData Texture;
		BufferViewData Buffer;
	};

	ViewData()
		: Type(ViewResourceType::Unknown)
	{}

	ViewData(Texture_t tex, RenderFormat format, uint32_t depthOrArraySize, uint32_t mipSlice)
		: Texture(tex, format, depthOrArraySize, mipSlice)
		, Type(ViewResourceType::Texture)
	{}

	ViewData(StructuredBuffer_t buf, uint32_t firstElem, uint32_t numElems, uint32_t stride)
		: Buffer(buf, firstElem, numElems, stride)
		, Type(ViewResourceType::StructuredBuffer)
	{}
};

IDArray<ShaderResourceView_t, ViewData> g_SRVs;
IDArray<UnorderedAccessView_t, ViewData> g_UAVs;
IDArray<RenderTargetView_t, ViewData> g_RTVs;
IDArray<DepthStencilView_t, ViewData> g_DSVs;

std::mutex SrvMutex;
std::mutex UavMutex;
std::mutex RtvMutex;
std::mutex DsvMutex;

ShaderResourceView_t CreateSrv_Lock(ViewData&& viewData)
{
	std::scoped_lock lock(SrvMutex);

	return g_SRVs.Create(viewData);
}

UnorderedAccessView_t CreateUav_Lock(ViewData&& viewData)
{
	std::scoped_lock lock(UavMutex);

	return g_UAVs.Create(viewData);
}

RenderTargetView_t CreateRtv_Lock(ViewData&& viewData)
{
	std::scoped_lock lock(RtvMutex);

	return g_RTVs.Create(viewData);
}

DepthStencilView_t CreateDsv_Lock(ViewData&& viewData)
{
	std::scoped_lock lock(DsvMutex);

	return g_DSVs.Create(viewData);
}

void ReleaseSrv_Lock(ShaderResourceView_t srv)
{
	std::scoped_lock lock(SrvMutex);

	g_SRVs.Release(srv);
}

void ReleaseUav_Lock(UnorderedAccessView_t uav)
{
	std::scoped_lock lock(UavMutex);

	g_UAVs.Release(uav);
}

void ReleaseRtv_Lock(RenderTargetView_t rtv)
{
	std::scoped_lock lock(RtvMutex);

	g_RTVs.Release(rtv);
}

void ReleaseDsv_Lock(DepthStencilView_t dsv)
{
	std::scoped_lock lock(DsvMutex);

	g_DSVs.Release(dsv);
}

ShaderResourceView_t CreateTextureSRV(Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t mipLevels, uint32_t depthOrArraySize)
{
	ShaderResourceView_t srv = CreateSrv_Lock(ViewData(tex, format, depthOrArraySize, mipLevels));

	if (!CreateTextureSRVImpl(srv, tex, format, dim, mipLevels, depthOrArraySize))
	{
		ReleaseSrv_Lock(srv);
		return ShaderResourceView_t::INVALID;
	}

	return srv;
}

UnorderedAccessView_t CreateTextureUAV(Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize, uint32_t mipSlice)
{
	UnorderedAccessView_t uav = CreateUav_Lock(ViewData(tex, format, depthOrArraySize, mipSlice));

	if (!CreateTextureUAVImpl(uav, tex, format, dim, depthOrArraySize, mipSlice))
	{
		ReleaseUav_Lock(uav);
		return UnorderedAccessView_t::INVALID;
	}

	return uav;
}

RenderTargetView_t CreateTextureRTV(Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize)
{
	RenderTargetView_t rtv = CreateRtv_Lock(ViewData(tex, format, depthOrArraySize, -1));

	if (!CreateTextureRTVImpl(rtv, tex, format, dim, depthOrArraySize))
	{
		ReleaseRtv_Lock(rtv);
		return RenderTargetView_t::INVALID;
	}

	return rtv;
}

DepthStencilView_t CreateTextureDSV(Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize)
{
	DepthStencilView_t dsv = CreateDsv_Lock(ViewData(tex, format, depthOrArraySize, -1));

	if (!CreateTextureDSVImpl(dsv, tex, format, dim, depthOrArraySize))
	{
		ReleaseDsv_Lock(dsv);
		return DepthStencilView_t::INVALID;
	}

	return dsv;
}

ShaderResourceView_t CreateTextureSRV(Texture_t tex)
{
	if (const TextureCreateDescEx* TexDesc = GetTextureDesc(tex))
	{
		if (!HasEnumFlags(TexDesc->Flags, RenderResourceFlags::SRV))
		{
			return ShaderResourceView_t::INVALID;
		}

		return CreateTextureSRV(tex, TexDesc->ResourceFormat, TexDesc->Dimension, TexDesc->MipCount, TexDesc->DepthOrArraySize);
	}
	return ShaderResourceView_t::INVALID;
}

UnorderedAccessView_t CreateTextureUAV(Texture_t tex)
{
	if (const TextureCreateDescEx* TexDesc = GetTextureDesc(tex))
	{
		if (!HasEnumFlags(TexDesc->Flags, RenderResourceFlags::UAV))
		{
			return UnorderedAccessView_t::INVALID;
		}

		return CreateTextureUAV(tex, TexDesc->ResourceFormat, TexDesc->Dimension, TexDesc->DepthOrArraySize);
	}
	return UnorderedAccessView_t::INVALID;
}

RenderTargetView_t CreateTextureRTV(Texture_t tex)
{
	if (const TextureCreateDescEx* TexDesc = GetTextureDesc(tex))
	{
		if (!HasEnumFlags(TexDesc->Flags, RenderResourceFlags::RTV))
		{
			return RenderTargetView_t::INVALID;
		}

		return CreateTextureRTV(tex, TexDesc->ResourceFormat, TexDesc->Dimension, TexDesc->DepthOrArraySize);
	}
	return RenderTargetView_t::INVALID;
}

DepthStencilView_t CreateTextureDSV(Texture_t tex, RenderFormat depthFormat)
{
	if (const TextureCreateDescEx* TexDesc = GetTextureDesc(tex))
	{
		if (!HasEnumFlags(TexDesc->Flags, RenderResourceFlags::DSV))
		{
			return DepthStencilView_t::INVALID;
		}

		return CreateTextureDSV(tex, depthFormat, TexDesc->Dimension, TexDesc->DepthOrArraySize);
	}
	return DepthStencilView_t::INVALID;
}

ShaderResourceView_t CreateStructuredBufferSRV(StructuredBuffer_t buf, uint32_t firstElem, uint32_t numElems, uint32_t stride)
{
	ShaderResourceView_t srv = CreateSrv_Lock(ViewData(buf, firstElem, numElems, stride));

	if (!CreateStructuredBufferSRVImpl(srv, buf, firstElem, numElems, stride))
	{
		ReleaseSrv_Lock(srv);
		return ShaderResourceView_t::INVALID;
	}

	return srv;
}

UnorderedAccessView_t CreateStructuredBufferUAV(StructuredBuffer_t buf, uint32_t firstElem, uint32_t numElems, uint32_t stride)
{
	UnorderedAccessView_t uav = CreateUav_Lock(ViewData(buf, firstElem, numElems, stride));

	if (!CreateStructuredBufferUAVImpl(uav, buf, firstElem, numElems, stride))
	{
		ReleaseUav_Lock(uav);
		return UnorderedAccessView_t::INVALID;
	}

	return uav;
}

static RenderFormat GetViewDataFormat(const ViewData* const data)
{
	if (data && data->Type == ViewResourceType::Texture)
		return data->Texture.Format;

	return RenderFormat::UNKNOWN;
}

ShaderResourceView_t AllocSRV(RenderFormat format, TextureDimension dim, uint32_t mipLevels, uint32_t depthOrArraySize)
{
	ShaderResourceView_t srv = CreateSrv_Lock(ViewData(Texture_t::INVALID, format, depthOrArraySize, -1));

	if (!CreateTextureSRVImpl(srv, Texture_t::INVALID, format, dim, depthOrArraySize, mipLevels))
	{
		ReleaseSrv_Lock(srv);
		return ShaderResourceView_t::INVALID;
	}

	return srv;
}

UnorderedAccessView_t AllocUAV(RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize)
{
	UnorderedAccessView_t uav = CreateUav_Lock(ViewData(Texture_t::INVALID, format, depthOrArraySize, -1));

	if (!CreateTextureUAVImpl(uav, Texture_t::INVALID, format, dim, depthOrArraySize, 0))
	{
		ReleaseUav_Lock(uav);
		return UnorderedAccessView_t::INVALID;
	}

	return uav;
}

RenderTargetView_t AllocRTV(RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize)
{
	RenderTargetView_t rtv = CreateRtv_Lock(ViewData(Texture_t::INVALID, format, depthOrArraySize, -1));

	if (!CreateTextureRTVImpl(rtv, Texture_t::INVALID, format, dim, depthOrArraySize))
	{
		ReleaseRtv_Lock(rtv);
		return RenderTargetView_t::INVALID;
	}

	return rtv;
}

DepthStencilView_t AllocDSV(RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize)
{
	DepthStencilView_t dsv = CreateDsv_Lock(ViewData(Texture_t::INVALID, format, depthOrArraySize, -1));

	if (!CreateTextureDSVImpl(dsv, Texture_t::INVALID, format, dim, depthOrArraySize))
	{
		ReleaseDsv_Lock(dsv);
		return DepthStencilView_t::INVALID;
	}

	return dsv;
}

// TODO: Validate bind functions
void BindSRV(ShaderResourceView_t srv, Texture_t texture)
{
	if (srv != ShaderResourceView_t::INVALID)
		BindTextureSRVImpl(srv, texture);
}

void BindUAV(UnorderedAccessView_t uav, Texture_t texture)
{
	if (uav != UnorderedAccessView_t::INVALID)
		BindTextureUAVImpl(uav, texture);
}

void BindRTV(RenderTargetView_t rtv, Texture_t texture)
{
	if (rtv != RenderTargetView_t::INVALID)
		BindTextureRTVImpl(rtv, texture);
}

void BindDSV(DepthStencilView_t dsv, Texture_t texture)
{
	if (dsv != DepthStencilView_t::INVALID)
		BindTextureDSVImpl(dsv, texture);
}

RenderFormat GetSRVFormat(ShaderResourceView_t srv)
{
	auto lock = g_SRVs.ReadScopeLock();

	return GetViewDataFormat(g_SRVs.Get(srv));
}

RenderFormat GetUAVFormat(UnorderedAccessView_t uav)
{
	auto lock = g_UAVs.ReadScopeLock();

	return GetViewDataFormat(g_UAVs.Get(uav));
}

RenderFormat GetRTVFormat(RenderTargetView_t rtv)
{
	auto lock = g_RTVs.ReadScopeLock();

	return GetViewDataFormat(g_RTVs.Get(rtv));
}

RenderFormat GetDSVFormat(DepthStencilView_t dsv)
{
	auto lock = g_DSVs.ReadScopeLock();

	return GetViewDataFormat(g_DSVs.Get(dsv));
}

void RenderRef(ShaderResourceView_t srv)
{
	g_SRVs.AddRef(srv);
}

void RenderRef(UnorderedAccessView_t uav)
{
	g_UAVs.AddRef(uav);
}

void RenderRef(RenderTargetView_t rtv)
{
	g_RTVs.AddRef(rtv);
}

void RenderRef(DepthStencilView_t dsv)
{
	g_DSVs.AddRef(dsv);
}

void RenderRelease(ShaderResourceView_t srv)
{
	if (g_SRVs.Release(srv))
		DestroySRV(srv);
}

void RenderRelease(UnorderedAccessView_t uav)
{
	if (g_UAVs.Release(uav))
		DestroyUAV(uav);
}

void RenderRelease(RenderTargetView_t rtv)
{
	if (g_RTVs.Release(rtv))
		DestroyRTV(rtv);
}

void RenderRelease(DepthStencilView_t dsv)
{
	if (g_DSVs.Release(dsv))
		DestroyDSV(dsv);
}

size_t GetShaderResourceViewCount()
{
	return g_SRVs.UsedSize();
}

size_t GetUnorderedAccessViewCount()
{
	return g_UAVs.UsedSize();
}

size_t GetRenderTargetViewCount()
{
	return g_RTVs.UsedSize();
}

size_t GetDepthStencilViewCount()
{
	return g_DSVs.UsedSize();
}

uint32_t GetDescriptorIndex(ShaderResourceView_t srv)
{
	if (g_SRVs.Valid(srv))
	{
		return GetDescriptorIndexImpl(srv);
	}

	return 0u;
}

uint32_t GetDescriptorIndex(UnorderedAccessView_t uav)
{
	if (g_UAVs.Valid(uav))
	{
		return GetDescriptorIndexImpl(uav);
	}

	return 0u;
}

}
