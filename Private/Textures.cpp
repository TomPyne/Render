#include "Textures.h"
#include "Binding.h"
#include "IDArray.h"
#include "Impl/TexturesImpl.h"
#include "TextureInfo.h"

#include <algorithm>

namespace rl
{

struct TextureData
{
    TextureCreateDescEx Desc;
};

IDArray<Texture_t, TextureData> g_Textures;

TextureCreateDescEx::TextureCreateDescEx(const TextureCreateDesc& Desc)
{
    Width = Desc.Width;
    Height = Desc.Height;
    Flags = Desc.Flags;
    DepthOrArraySize = 1;
    Data = Desc.Data;
    Dimension = TextureDimension::TEX2D;
    ResourceFormat = Desc.Format;
    Usage = ResourceUsage::DEFAULT;
    CpuAccess = TextureCPUAccess::NONE;
    InitialState = ResourceTransitionState::COMMON;
}

Texture_t CreateTexture(const void* const data, RenderFormat format, uint32_t width, uint32_t height)
{
    TextureCreateDesc desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = format;
    desc.Flags = RenderResourceFlags::SRV;

    MipData mipData{data, format, width, height};

    desc.Data = &mipData;

    return CreateTexture(desc);
}

Texture_t CreateTexture(const TextureCreateDesc& desc)
{
    TextureCreateDescEx descEx = {};
    descEx.Width = desc.Width;
    descEx.Height = desc.Height;
    descEx.Flags = desc.Flags;
    descEx.DepthOrArraySize = 1;
    descEx.Data = desc.Data;
    descEx.Dimension = TextureDimension::TEX2D;
    descEx.ResourceFormat = desc.Format;
    descEx.Usage = ResourceUsage::DEFAULT;
    descEx.CpuAccess = TextureCPUAccess::NONE;
    descEx.InitialState = ResourceTransitionState::COMMON;
    descEx.DebugName = desc.DebugName;

    return CreateTextureEx(descEx);
}

Texture_t CreateTextureEx(const TextureCreateDescEx& desc)
{
    Texture_t newTex = g_Textures.Create();

    if (!CreateTextureImpl(newTex, desc))
    {
        g_Textures.Release(newTex);
        return Texture_t::INVALID;
    }

    {
        auto lock = g_Textures.ReadScopeLock();

        TextureData* data = g_Textures.Get(newTex);
        data->Desc = desc;
    }

    return newTex;
}

Texture_t AllocTexture()
{
    Texture_t newTex = g_Textures.Create();

    AllocTextureImpl(newTex);

    return newTex;
}

const TextureCreateDescEx* GetTextureDesc(Texture_t tex)
{
    auto lock = g_Textures.ReadScopeLock();

    if (const TextureData* data = g_Textures.Get(tex))
    {
        return &data->Desc;
    }

    return nullptr;
}

void UpdateTexture(Texture_t tex, const void* const data, uint32_t width, uint32_t height, RenderFormat format)
{
    // TODO Multithread: This is an expensive lock, we should enqueue the update instead
    auto lock = g_Textures.ReadScopeLock();

    if (TextureData* data = g_Textures.Get(tex))
        UpdateTextureImpl(tex, data, width, height, format);
}

void RenderRef(Texture_t tex)
{
    g_Textures.AddRef(tex);
}

void RenderRelease(Texture_t tex)
{
    if (g_Textures.Release(tex))
    {
        DestroyTexture(tex);
    }
}

bool Textures_SupportsDescriptors(Texture_t tex, RenderResourceFlags flags)
{
    auto lock = g_Textures.ReadScopeLock();

    if (const TextureData* data = g_Textures.Get(tex))
    {
        return (data->Desc.Flags & flags) == flags;
    }

    return false;
}

void GetTextureDims(Texture_t tex, uint32_t* w, uint32_t* h)
{
    auto lock = g_Textures.ReadScopeLock();

    if (TextureData* data = g_Textures.Get(tex))
    {
        *w = data->Desc.Width;
        *h = data->Desc.Height;
    }
}

size_t Texture_GetTextureCount()
{
    return g_Textures.UsedSize();
}

MipData::MipData(const void* _data, RenderFormat format, uint32_t width, uint32_t height)
{
    Data = _data;
    CalculateTexturePitch(format, width, height, &RowPitch, &SlicePitch);
}

}