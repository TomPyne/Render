#include "Impl/TexturesImpl.h"

#include "RenderImpl.h"
#include "SparseArray.h"

namespace rl
{

SparseArray<ComPtr<ID3D11Resource>, Texture_t> g_DxTextures;

static UINT Dx11_CpuAccessFlag(TextureCPUAccess access)
{
	UINT flag = 0u;

	if ((access & TextureCPUAccess::WRITE) != TextureCPUAccess::NONE)
		flag |= D3D11_CPU_ACCESS_WRITE;

	if ((access & TextureCPUAccess::READ) != TextureCPUAccess::NONE)
		flag |= D3D11_CPU_ACCESS_READ;

	return flag;
}

bool AllocTextureImpl(Texture_t tex)
{
	g_DxTextures.Alloc(tex);

	return true;
}

bool CreateTextureImpl(Texture_t tex, const TextureCreateDescEx& desc)
{
	auto& dxTex = g_DxTextures.Alloc(tex);

	const UINT bindFlags = Dx11_BindFlags(desc.Flags);
	const DXGI_FORMAT resourceFormat = Dx11_Format(desc.ResourceFormat);
	const D3D11_USAGE usage = Dx11_Usage(desc.Usage);
	const UINT cpuAccessFlags = Dx11_CpuAccessFlag(desc.CpuAccess);

	std::unique_ptr<D3D11_SUBRESOURCE_DATA[]> subRes = nullptr; 
	if (desc.Data)
	{
		subRes = std::make_unique<D3D11_SUBRESOURCE_DATA[]>(desc.MipCount * desc.DepthOrArraySize);
		for (size_t i = 0; i < desc.MipCount * desc.DepthOrArraySize; i++)
		{
			subRes[i].pSysMem = desc.Data[i].Data;
			subRes[i].SysMemPitch = static_cast<UINT>(desc.Data[i].RowPitch);
			subRes[i].SysMemSlicePitch = static_cast<UINT>(desc.Data[i].SlicePitch);
		}
	}

	switch (desc.Dimension)
	{
	case TextureDimension::TEX1D:
	{
		D3D11_TEXTURE1D_DESC dxDesc;
		dxDesc.Width = static_cast<UINT>(desc.Width);
		dxDesc.MipLevels = static_cast<UINT>(desc.MipCount);
		dxDesc.ArraySize = static_cast<UINT>(desc.DepthOrArraySize);
		dxDesc.Format = resourceFormat;
		dxDesc.Usage = usage;
		dxDesc.BindFlags = bindFlags;
		dxDesc.CPUAccessFlags = cpuAccessFlags;
		dxDesc.MiscFlags = 0;

		ComPtr<ID3D11Texture1D> dxTex1D;
		if (FAILED(g_render.Device->CreateTexture1D(&dxDesc, subRes.get(), &dxTex1D)))
			return false;

		dxTex = dxTex1D;
	}
	break;
	case TextureDimension::TEX2D:
	case TextureDimension::CUBEMAP:
	{
		D3D11_TEXTURE2D_DESC dxDesc;
		dxDesc.Width = static_cast<UINT>(desc.Width);
		dxDesc.Height = static_cast<UINT>(desc.Height);
		dxDesc.MipLevels = static_cast<UINT>(desc.MipCount);
		dxDesc.ArraySize = static_cast<UINT>(desc.DepthOrArraySize);
		dxDesc.Format = resourceFormat;
		dxDesc.SampleDesc.Count = 1;
		dxDesc.SampleDesc.Quality = 0;
		dxDesc.Usage = usage;
		dxDesc.BindFlags = bindFlags;
		dxDesc.CPUAccessFlags = cpuAccessFlags;
		dxDesc.MiscFlags = desc.Dimension == TextureDimension::CUBEMAP ? D3D11_RESOURCE_MISC_TEXTURECUBE : 0;

		ComPtr<ID3D11Texture2D> dxTex2D;
		if (FAILED(g_render.Device->CreateTexture2D(&dxDesc, subRes.get(), &dxTex2D)))
			return false;

		dxTex = dxTex2D;
	}
	break;
	case TextureDimension::TEX3D:
	{
		D3D11_TEXTURE3D_DESC dxDesc;
		dxDesc.Width = static_cast<UINT>(desc.Width);
		dxDesc.Height = static_cast<UINT>(desc.Height);
		dxDesc.Depth = static_cast<UINT>(desc.DepthOrArraySize);
		dxDesc.MipLevels = static_cast<UINT>(desc.MipCount);
		dxDesc.Format = resourceFormat;
		dxDesc.Usage = usage;
		dxDesc.BindFlags = bindFlags;
		dxDesc.CPUAccessFlags = cpuAccessFlags;
		dxDesc.MiscFlags = 0;

		ComPtr<ID3D11Texture3D> dxTex3D;
		if (FAILED(g_render.Device->CreateTexture3D(&dxDesc, subRes.get(), &dxTex3D)))
			return false;

		dxTex = dxTex3D;
	}
	break;
	default:
		return false;
	};

	return true;
}

bool UpdateTextureImpl(Texture_t tex, const void* const data, uint32_t width, uint32_t height, RenderFormat format)
{
	if (!g_DxTextures.Valid(tex))
		return false;

	D3D11_TEXTURE2D_DESC td;
	ZeroMemory(&td, sizeof(td));

	td.Width = (UINT)width;
	td.Height = (UINT)height;
	td.MipLevels = 1;
	td.ArraySize = 1;
	td.Format = Dx11_Format(format);
	td.Usage = D3D11_USAGE_STAGING;

	D3D11_SUBRESOURCE_DATA subRes = {};
	subRes.pSysMem = data;
	Dx11_CalculatePitch(td.Format, td.Width, td.Height, &subRes.SysMemPitch, &subRes.SysMemSlicePitch);

	ComPtr<ID3D11Texture2D> stagingTex;
	if (!SUCCEEDED(g_render.Device->CreateTexture2D(&td, &subRes, &stagingTex)))
		return false;

	g_render.DeviceContext->CopyResource(g_DxTextures[tex].Get(), stagingTex.Get());

	return true;
}

bool IsTextureUploadCompleteImpl(Texture_t tex)
{
	// The immediate context orders the initial data before any later use
	return g_DxTextures.Valid(tex);
}

void DestroyTexture(Texture_t tex)
{
	g_DxTextures[tex] = nullptr;
}

ID3D11Resource* Dx11_GetTexture(Texture_t tex)
{
	return g_DxTextures.Valid(tex) ? g_DxTextures[tex].Get() : nullptr;
}

TextureResourceAccessScope::TextureResourceAccessScope(Texture_t resource, TextureResourceAccessMethod method, uint32_t subResourceIndex)
	: MappedTex(resource)
	, SubResIdx(subResourceIndex)
{
	if (!g_DxTextures.Valid(MappedTex))
		return;

	ID3D11Resource* dxRes = g_DxTextures[MappedTex].Get();

	D3D11_MAP mapType;
	if (method == TextureResourceAccessMethod::Read)
		mapType = D3D11_MAP_READ;
	else if (method == TextureResourceAccessMethod::Write)
		mapType = D3D11_MAP_WRITE;
	else if (method == TextureResourceAccessMethod::ReadWrite)
		mapType = D3D11_MAP_READ_WRITE;
	else
		return;

	D3D11_MAPPED_SUBRESOURCE mapped;
	if(FAILED(g_render.DeviceContext->Map(dxRes, subResourceIndex, mapType, 0, &mapped)))
		return;

	Ptr = mapped.pData;
	RowPitch = mapped.RowPitch;
	DepthPitch = mapped.DepthPitch;
}

TextureResourceAccessScope::~TextureResourceAccessScope()
{
	if(Ptr)
		g_render.DeviceContext->Unmap(g_DxTextures[MappedTex].Get(), SubResIdx);
}

void Dx11_SetTextureResource(Texture_t tex, const ComPtr<ID3D11Resource>& resource)
{
	if (g_DxTextures.Valid(tex))
	{
		g_DxTextures[tex] = resource;
	}
}

}