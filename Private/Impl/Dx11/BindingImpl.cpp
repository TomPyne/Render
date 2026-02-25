#include "Impl/BindingImpl.h"

#include "SparseArray.h"
#include "RenderImpl.h"
#include "Textures.h"

namespace rl
{

struct SrvDesc
{
	D3D11_SHADER_RESOURCE_VIEW_DESC Desc = {};
	ComPtr<ID3D11ShaderResourceView> DxSrv = nullptr;
};

struct UavDesc
{
	D3D11_UNORDERED_ACCESS_VIEW_DESC Desc = {};
	ComPtr<ID3D11UnorderedAccessView> DxUav = nullptr;
};

struct RtvDesc
{
	D3D11_RENDER_TARGET_VIEW_DESC Desc = {};
	ComPtr<ID3D11RenderTargetView> DxRtv = nullptr;
};

struct DsvDesc
{
	D3D11_DEPTH_STENCIL_VIEW_DESC Desc = {};
	ComPtr<ID3D11DepthStencilView> DxDsv = nullptr;
};

SparseArray<SrvDesc, ShaderResourceView_t> g_Srvs;
SparseArray<UavDesc, UnorderedAccessView_t> g_Uavs;
SparseArray<RtvDesc, RenderTargetView_t> g_Rtvs;
SparseArray<DsvDesc, DepthStencilView_t> g_Dsvs;

bool CreateTextureSRVImpl(ShaderResourceView_t srv, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t mipLevels, uint32_t depthOrArraySize)
{
	auto& dxSRV = g_Srvs.Alloc(srv);

	D3D11_SHADER_RESOURCE_VIEW_DESC& desc = dxSRV.Desc;

	desc.Format = Dx11_Format(format);

	if (dim == TextureDimension::TEX1D)
	{
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		desc.Texture1D.MostDetailedMip = 0u;
		desc.Texture1D.MipLevels = mipLevels;
	}
	else if (dim == TextureDimension::TEX1D_ARRAY)
	{
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
		desc.Texture1DArray.MostDetailedMip = 0u;
		desc.Texture1DArray.MipLevels = mipLevels;
		desc.Texture1DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::TEX2D)
	{
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MostDetailedMip = 0u;
		desc.Texture2D.MipLevels = mipLevels;
	}
	else if (dim == TextureDimension::TEX2D_ARRAY)
	{
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MostDetailedMip = 0u;
		desc.Texture2DArray.MipLevels = mipLevels;
		desc.Texture2DArray.FirstArraySlice = 0u;
		desc.Texture2DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::CUBEMAP)
	{
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
		desc.TextureCube.MostDetailedMip = 0u;
		desc.TextureCube.MipLevels = mipLevels;
	}
	else if (dim == TextureDimension::CUBEMAP_ARRAY)
	{
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
		desc.TextureCubeArray.MostDetailedMip = 0u;
		desc.TextureCubeArray.MipLevels = mipLevels;
		desc.TextureCubeArray.First2DArrayFace = 0u;
		desc.TextureCubeArray.NumCubes = depthOrArraySize;
	}
	else if (dim == TextureDimension::TEX3D)
	{
		desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MostDetailedMip = 0u;
		desc.Texture3D.MipLevels = mipLevels;
	}
	else
	{
		g_Srvs.Free(srv);
		return false;
	}

	ID3D11Resource* res = Dx11_GetTexture(tex);
	if (!res)
		return true;

	return SUCCEEDED(g_render.Device->CreateShaderResourceView(res, &desc, &dxSRV.DxSrv));
}

bool CreateTextureUAVImpl(UnorderedAccessView_t uav, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize, uint32_t mipSlice)
{
	auto& dxUav = g_Uavs.Alloc(uav);

	D3D11_UNORDERED_ACCESS_VIEW_DESC& desc = dxUav.Desc;

	desc.Format = Dx11_Format(format);

	if (dim == TextureDimension::TEX1D)
	{
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
		desc.Texture1D.MipSlice = mipSlice;
	}
	else if (dim == TextureDimension::TEX1D_ARRAY)
	{
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
		desc.Texture1DArray.MipSlice = mipSlice;
		desc.Texture1DArray.FirstArraySlice = 0u;
		desc.Texture1DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::TEX2D)
	{
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = mipSlice;
	}
	else if (dim == TextureDimension::TEX2D_ARRAY)
	{
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = mipSlice;
		desc.Texture2DArray.FirstArraySlice = mipSlice;
		desc.Texture2DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::CUBEMAP)
	{
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = mipSlice;
		desc.Texture2DArray.FirstArraySlice = 0u;
		desc.Texture2DArray.ArraySize = 6u;
	}
	else if (dim == TextureDimension::CUBEMAP_ARRAY)
	{
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = mipSlice;
		desc.Texture2DArray.FirstArraySlice = 0u;
		desc.Texture2DArray.ArraySize = 6u * depthOrArraySize;
	}
	else if (dim == TextureDimension::TEX3D)
	{
		desc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MipSlice = mipSlice;
		desc.Texture3D.FirstWSlice = 0u;
		desc.Texture3D.WSize = -1;
	}
	else
	{
		g_Uavs.Free(uav);

		return false;
	}

	if (tex != Texture_t::INVALID)
	{
		return SUCCEEDED(g_render.Device->CreateUnorderedAccessView(Dx11_GetTexture(tex), &desc, &dxUav.DxUav));
	}

	return true;
}

bool CreateTextureRTVImpl(RenderTargetView_t rtv, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize)
{
	auto& dxRtv = g_Rtvs.Alloc(rtv);

	D3D11_RENDER_TARGET_VIEW_DESC& desc = dxRtv.Desc;

	desc.Format = Dx11_Format(format);

	if (dim == TextureDimension::TEX1D)
	{
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
		desc.Texture1D.MipSlice = 0u;
	}
	else if (dim == TextureDimension::TEX1D_ARRAY)
	{
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
		desc.Texture1DArray.MipSlice = 0u;
		desc.Texture1DArray.FirstArraySlice = 0u;
		desc.Texture1DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::TEX2D)
	{
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0u;
	}
	else if (dim == TextureDimension::TEX2D_ARRAY)
	{
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = 0u;
		desc.Texture2DArray.FirstArraySlice = 0u;
		desc.Texture2DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::CUBEMAP)
	{
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = 0u;
		desc.Texture2DArray.FirstArraySlice = 0u;
		desc.Texture2DArray.ArraySize = 6u;
	}
	else if (dim == TextureDimension::CUBEMAP_ARRAY)
	{
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = 0u;
		desc.Texture2DArray.FirstArraySlice = 0u;
		desc.Texture2DArray.ArraySize = 6u * depthOrArraySize;
	}
	else if (dim == TextureDimension::TEX3D)
	{
		desc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
		desc.Texture3D.MipSlice = 0u;
		desc.Texture3D.FirstWSlice = 0u;
		desc.Texture3D.WSize = -1;
	}
	else
	{
		g_Rtvs.Free(rtv);

		return false;
	}

	if (tex != Texture_t::INVALID)
	{
		return SUCCEEDED(g_render.Device->CreateRenderTargetView(Dx11_GetTexture(tex), &desc, &dxRtv.DxRtv));
	}

	return true;	
}

bool CreateTextureDSVImpl(DepthStencilView_t dsv, Texture_t tex, RenderFormat format, TextureDimension dim, uint32_t depthOrArraySize)
{
	auto& dxDsv = g_Dsvs.Alloc(dsv);

	D3D11_DEPTH_STENCIL_VIEW_DESC& desc = dxDsv.Desc;

	desc.Format = Dx11_Format(format);

	if (dim == TextureDimension::TEX1D)
	{
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
		desc.Texture1D.MipSlice = 0u;
	}
	else if (dim == TextureDimension::TEX1D_ARRAY)
	{
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
		desc.Texture1DArray.MipSlice = 0u;
		desc.Texture1DArray.FirstArraySlice = 0u;
		desc.Texture1DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::TEX2D)
	{
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		desc.Texture2D.MipSlice = 0u;
	}
	else if (dim == TextureDimension::TEX2D_ARRAY)
	{
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = 0u;
		desc.Texture2DArray.FirstArraySlice = 0u;
		desc.Texture2DArray.ArraySize = depthOrArraySize;
	}
	else if (dim == TextureDimension::CUBEMAP)
	{
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = 0u;
		desc.Texture2DArray.FirstArraySlice = 0u;
		desc.Texture2DArray.ArraySize = 6u;
	}
	else if (dim == TextureDimension::CUBEMAP_ARRAY)
	{
		desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
		desc.Texture2DArray.MipSlice = 0u;
		desc.Texture2DArray.FirstArraySlice = 0u;
		desc.Texture2DArray.ArraySize = 6u * depthOrArraySize;
	}
	else
	{
		g_Dsvs.Free(dsv);

		return false;
	}

	if (tex != Texture_t::INVALID)
	{
		return SUCCEEDED(g_render.Device->CreateDepthStencilView(Dx11_GetTexture(tex), &desc, &dxDsv.DxDsv));
	}

	return true;
}

void BindTextureSRVImpl(ShaderResourceView_t srv, Texture_t tex)
{
	if (g_Srvs.Valid(srv))
	{
		DXENSURE(g_render.Device->CreateShaderResourceView(Dx11_GetTexture(tex), &g_Srvs[srv].Desc, &g_Srvs[srv].DxSrv));
	}
}

void BindTextureUAVImpl(UnorderedAccessView_t uav, Texture_t tex)
{
	if (g_Uavs.Valid(uav))
	{
		DXENSURE(g_render.Device->CreateUnorderedAccessView(Dx11_GetTexture(tex), &g_Uavs[uav].Desc, &g_Uavs[uav].DxUav));
	}
}

void BindTextureRTVImpl(RenderTargetView_t rtv, Texture_t tex)
{
	if (g_Rtvs.Valid(rtv))
	{
		DXENSURE(g_render.Device->CreateRenderTargetView(Dx11_GetTexture(tex), &g_Rtvs[rtv].Desc, &g_Rtvs[rtv].DxRtv));
	}
}

void BindTextureDSVImpl(DepthStencilView_t dsv, Texture_t tex)
{
	if (g_Dsvs.Valid(dsv))
	{
		DXENSURE(g_render.Device->CreateDepthStencilView(Dx11_GetTexture(tex), &g_Dsvs[dsv].Desc, &g_Dsvs[dsv].DxDsv));
	}
}

bool CreateStructuredBufferSRVImpl(ShaderResourceView_t srv, StructuredBuffer_t buf, uint32_t firstElement, uint32_t numElements, uint32_t structureByteStride)
{
	ID3D11Resource* res = Dx11_GetStructuredBuffer(buf);
	if (!res)
		return false;

	auto& dxSrv = g_Srvs.Alloc(srv);

	D3D11_SHADER_RESOURCE_VIEW_DESC& desc = dxSrv.Desc;

	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;

	desc.Buffer.FirstElement = (UINT)firstElement;
	desc.Buffer.NumElements = (UINT)numElements;

	return SUCCEEDED(g_render.Device->CreateShaderResourceView(res, &desc, &dxSrv.DxSrv));
}

bool CreateStructuredBufferUAVImpl(UnorderedAccessView_t uav, StructuredBuffer_t buf, uint32_t firstElement, uint32_t numElements, uint32_t structureByteStrude)
{
	ID3D11Resource* res = Dx11_GetStructuredBuffer(buf);
	if (!res)
		return false;

	auto& dxUav = g_Uavs.Alloc(uav);

	D3D11_UNORDERED_ACCESS_VIEW_DESC& desc = dxUav.Desc;

	desc.Format = DXGI_FORMAT_UNKNOWN;
	desc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;

	desc.Buffer.FirstElement = (UINT)firstElement;
	desc.Buffer.NumElements = (UINT)numElements;
	desc.Buffer.Flags = 0u;

	return SUCCEEDED(g_render.Device->CreateUnorderedAccessView(res, &desc, &dxUav.DxUav));
}

void DestroySRV(ShaderResourceView_t srv)
{
	g_Srvs.Free(srv);
}

void DestroyUAV(UnorderedAccessView_t uav)
{
	g_Uavs.Free(uav);
}

void DestroyRTV(RenderTargetView_t rtv)
{
	g_Rtvs.Free(rtv);
}

void DestroyDSV(DepthStencilView_t dsv)
{
	g_Dsvs.Free(dsv);
}

ID3D11ShaderResourceView* Dx11_GetShaderResourceView(ShaderResourceView_t srv)
{
	return g_Srvs.Valid(srv) ? g_Srvs[srv].DxSrv.Get() : nullptr;
}

ID3D11UnorderedAccessView* Dx11_GetUnorderedAccessView(UnorderedAccessView_t uav)
{
	return g_Uavs.Valid(uav) ? g_Uavs[uav].DxUav.Get() : nullptr;
}

ID3D11RenderTargetView* Dx11_GetRenderTargetView(RenderTargetView_t rtv)
{
	return g_Rtvs.Valid(rtv) ? g_Rtvs[rtv].DxRtv.Get() : nullptr;
}

ID3D11DepthStencilView* Dx11_GetDepthStencilView(DepthStencilView_t dsv)
{
	return g_Dsvs.Valid(dsv) ? g_Dsvs[dsv].DxDsv.Get() : nullptr;
}

uint32_t GetDescriptorIndexImpl(ShaderResourceView_t srv)
{
	assert(0);
	return 0;
}

uint32_t GetDescriptorIndexImpl(UnorderedAccessView_t uav)
{
	assert(0);
	return 0;
}

}