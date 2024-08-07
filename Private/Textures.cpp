#include "Textures.h"
#include "Binding.h"
#include "IDArray.h"
#include "Impl/TexturesImpl.h"

#include <algorithm>

namespace tpr
{

struct TextureData
{
    TextureCreateDescEx Desc;
};

IDArray<Texture_t, TextureData> g_Textures;

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

size_t BitsPerPixel(RenderFormat format)
{
    switch (format)
    {
    case RenderFormat::R32G32B32A32_TYPELESS:
    case RenderFormat::R32G32B32A32_FLOAT:
    case RenderFormat::R32G32B32A32_UINT:
    case RenderFormat::R32G32B32A32_SINT:
        return 128;

    case RenderFormat::R32G32B32_TYPELESS:
    case RenderFormat::R32G32B32_FLOAT:
    case RenderFormat::R32G32B32_UINT:
    case RenderFormat::R32G32B32_SINT:
        return 96;

    case RenderFormat::R16G16B16A16_TYPELESS:
    case RenderFormat::R16G16B16A16_FLOAT:
    case RenderFormat::R16G16B16A16_UNORM:
    case RenderFormat::R16G16B16A16_UINT:
    case RenderFormat::R16G16B16A16_SNORM:
    case RenderFormat::R16G16B16A16_SINT:
    case RenderFormat::R32G32_TYPELESS:
    case RenderFormat::R32G32_FLOAT:
    case RenderFormat::R32G32_UINT:
    case RenderFormat::R32G32_SINT:
    case RenderFormat::R32G8X24_TYPELESS:
    case RenderFormat::D32_FLOAT_S8X24_UINT:
    case RenderFormat::R32_FLOAT_X8X24_TYPELESS:
    case RenderFormat::X32_TYPELESS_G8X24_UINT:
    case RenderFormat::Y416:
    case RenderFormat::Y210:
    case RenderFormat::Y216:
        return 64;

    case RenderFormat::R10G10B10A2_TYPELESS:
    case RenderFormat::R10G10B10A2_UNORM:
    case RenderFormat::R10G10B10A2_UINT:
    case RenderFormat::R11G11B10_FLOAT:
    case RenderFormat::R8G8B8A8_TYPELESS:
    case RenderFormat::R8G8B8A8_UNORM:
    case RenderFormat::R8G8B8A8_UNORM_SRGB:
    case RenderFormat::R8G8B8A8_UINT:
    case RenderFormat::R8G8B8A8_SNORM:
    case RenderFormat::R8G8B8A8_SINT:
    case RenderFormat::R16G16_TYPELESS:
    case RenderFormat::R16G16_FLOAT:
    case RenderFormat::R16G16_UNORM:
    case RenderFormat::R16G16_UINT:
    case RenderFormat::R16G16_SNORM:
    case RenderFormat::R16G16_SINT:
    case RenderFormat::R32_TYPELESS:
    case RenderFormat::D32_FLOAT:
    case RenderFormat::R32_FLOAT:
    case RenderFormat::R32_UINT:
    case RenderFormat::R32_SINT:
    case RenderFormat::R24G8_TYPELESS:
    case RenderFormat::D24_UNORM_S8_UINT:
    case RenderFormat::R24_UNORM_X8_TYPELESS:
    case RenderFormat::X24_TYPELESS_G8_UINT:
    case RenderFormat::R9G9B9E5_SHAREDEXP:
    case RenderFormat::R8G8_B8G8_UNORM:
    case RenderFormat::G8R8_G8B8_UNORM:
    case RenderFormat::B8G8R8A8_UNORM:
    case RenderFormat::B8G8R8X8_UNORM:
    case RenderFormat::R10G10B10_XR_BIAS_A2_UNORM:
    case RenderFormat::B8G8R8A8_TYPELESS:
    case RenderFormat::B8G8R8A8_UNORM_SRGB:
    case RenderFormat::B8G8R8X8_TYPELESS:
    case RenderFormat::B8G8R8X8_UNORM_SRGB:
    case RenderFormat::AYUV:
    case RenderFormat::Y410:
    case RenderFormat::YUY2:
        return 32;

    case RenderFormat::P010:
    case RenderFormat::P016:
        return 24;

    case RenderFormat::R8G8_TYPELESS:
    case RenderFormat::R8G8_UNORM:
    case RenderFormat::R8G8_UINT:
    case RenderFormat::R8G8_SNORM:
    case RenderFormat::R8G8_SINT:
    case RenderFormat::R16_TYPELESS:
    case RenderFormat::R16_FLOAT:
    case RenderFormat::D16_UNORM:
    case RenderFormat::R16_UNORM:
    case RenderFormat::R16_UINT:
    case RenderFormat::R16_SNORM:
    case RenderFormat::R16_SINT:
    case RenderFormat::B5G6R5_UNORM:
    case RenderFormat::B5G5R5A1_UNORM:
    case RenderFormat::A8P8:
    case RenderFormat::B4G4R4A4_UNORM:
        return 16;

    case RenderFormat::NV12:
    case RenderFormat::OPAQUE_420:
    case RenderFormat::NV11:
        return 12;

    case RenderFormat::R8_TYPELESS:
    case RenderFormat::R8_UNORM:
    case RenderFormat::R8_UINT:
    case RenderFormat::R8_SNORM:
    case RenderFormat::R8_SINT:
    case RenderFormat::A8_UNORM:
    case RenderFormat::BC2_TYPELESS:
    case RenderFormat::BC2_UNORM:
    case RenderFormat::BC2_UNORM_SRGB:
    case RenderFormat::BC3_TYPELESS:
    case RenderFormat::BC3_UNORM:
    case RenderFormat::BC3_UNORM_SRGB:
    case RenderFormat::BC5_TYPELESS:
    case RenderFormat::BC5_UNORM:
    case RenderFormat::BC5_SNORM:
    case RenderFormat::BC6H_TYPELESS:
    case RenderFormat::BC6H_UF16:
    case RenderFormat::BC6H_SF16:
    case RenderFormat::BC7_TYPELESS:
    case RenderFormat::BC7_UNORM:
    case RenderFormat::BC7_UNORM_SRGB:
    case RenderFormat::AI44:
    case RenderFormat::IA44:
    case RenderFormat::P8:
        return 8;

    case RenderFormat::R1_UNORM:
        return 1;

    case RenderFormat::BC1_TYPELESS:
    case RenderFormat::BC1_UNORM:
    case RenderFormat::BC1_UNORM_SRGB:
    case RenderFormat::BC4_TYPELESS:
    case RenderFormat::BC4_UNORM:
    case RenderFormat::BC4_SNORM:
        return 4;

    default:
        return 0;
    }
}

void CalculateTexturePitch(RenderFormat format, uint32_t width, uint32_t height, size_t* rowPitch, size_t* slicePitch)
{
    uint64_t pitch = 0;
    uint64_t slice = 0;

    switch (format)
    {
    case RenderFormat::BC1_TYPELESS:
    case RenderFormat::BC1_UNORM:
    case RenderFormat::BC1_UNORM_SRGB:
    case RenderFormat::BC4_TYPELESS:
    case RenderFormat::BC4_UNORM:
    case RenderFormat::BC4_SNORM:
    {
        const uint64_t nbw = max(1u, (uint64_t(width) + 3u) / 4u);
        const uint64_t nbh = max(1u, (uint64_t(height) + 3u) / 4u);
        pitch = nbw * 8u;
        slice = pitch * nbh;
    }
    break;
    case RenderFormat::BC2_TYPELESS:
    case RenderFormat::BC2_UNORM:
    case RenderFormat::BC2_UNORM_SRGB:
    case RenderFormat::BC3_TYPELESS:
    case RenderFormat::BC3_UNORM:
    case RenderFormat::BC3_UNORM_SRGB:
    case RenderFormat::BC5_TYPELESS:
    case RenderFormat::BC5_UNORM:
    case RenderFormat::BC5_SNORM:
    case RenderFormat::BC6H_TYPELESS:
    case RenderFormat::BC6H_UF16:
    case RenderFormat::BC6H_SF16:
    case RenderFormat::BC7_TYPELESS:
    case RenderFormat::BC7_UNORM:
    case RenderFormat::BC7_UNORM_SRGB:
    {
        const uint64_t nbw = max(1u, (uint64_t(width) + 3u) / 4u);
        const uint64_t nbh = max(1u, (uint64_t(height) + 3u) / 4u);
        pitch = nbw * 16u;
        slice = pitch * nbh;
    }
    break;
    case RenderFormat::R8G8_B8G8_UNORM:
    case RenderFormat::G8R8_G8B8_UNORM:
    case RenderFormat::YUY2:
    {
        pitch = ((uint64_t(width) + 1u) >> 1) * 4u;
        slice = pitch * uint64_t(height);
    }
    break;
    case RenderFormat::Y210:
    case RenderFormat::Y216:
    {
        pitch = ((uint64_t(width) + 1u) >> 1) * 8u;
        slice = pitch * uint64_t(height);
    }
    break;
    case RenderFormat::NV12:
    case RenderFormat::OPAQUE_420:
    case RenderFormat::P010:
    case RenderFormat::P016:
        return; // Unsupported.
    default:
    {
        size_t bpp = BitsPerPixel(format);

        // Default byte alignment
        pitch = (uint64_t(width) * bpp + 7u) / 8u;
        slice = pitch * uint64_t(height);
    }
    break;
    }

    *rowPitch = static_cast<size_t>(pitch);
    *slicePitch = static_cast<size_t>(slice);
}

void GetTextureSurfaceInfo(uint32_t width, uint32_t height, RenderFormat format, size_t* outNumBytes, size_t* outRowBytes, size_t* outNumRows)
{
    uint64_t numBytes = 0;
    uint64_t rowBytes = 0;
    uint64_t numRows = 0;

    bool bc = false;
    bool packed = false;
    bool planar = false;
    size_t bpe = 0;
    switch (format)
    {
    case RenderFormat::BC1_TYPELESS:
    case RenderFormat::BC1_UNORM:
    case RenderFormat::BC1_UNORM_SRGB:
    case RenderFormat::BC4_TYPELESS:
    case RenderFormat::BC4_UNORM:
    case RenderFormat::BC4_SNORM:
        bc = true;
        bpe = 8;
        break;

    case RenderFormat::BC2_TYPELESS:
    case RenderFormat::BC2_UNORM:
    case RenderFormat::BC2_UNORM_SRGB:
    case RenderFormat::BC3_TYPELESS:
    case RenderFormat::BC3_UNORM:
    case RenderFormat::BC3_UNORM_SRGB:
    case RenderFormat::BC5_TYPELESS:
    case RenderFormat::BC5_UNORM:
    case RenderFormat::BC5_SNORM:
    case RenderFormat::BC6H_TYPELESS:
    case RenderFormat::BC6H_UF16:
    case RenderFormat::BC6H_SF16:
    case RenderFormat::BC7_TYPELESS:
    case RenderFormat::BC7_UNORM:
    case RenderFormat::BC7_UNORM_SRGB:
        bc = true;
        bpe = 16;
        break;

    case RenderFormat::R8G8_B8G8_UNORM:
    case RenderFormat::G8R8_G8B8_UNORM:
    case RenderFormat::YUY2:
        packed = true;
        bpe = 4;
        break;

    case RenderFormat::Y210:
    case RenderFormat::Y216:
        packed = true;
        bpe = 8;
        break;

    case RenderFormat::NV12:
    case RenderFormat::OPAQUE_420:
        planar = true;
        bpe = 2;
        break;

    case RenderFormat::P010:
    case RenderFormat::P016:
        planar = true;
        bpe = 4;
        break;

    default:
        break;
    }

    if (bc)
    {
        uint64_t numBlocksWide = 0;
        if (width > 0)
        {
            numBlocksWide = std::max<uint64_t>(1u, (uint64_t(width) + 3u) / 4u);
        }
        uint64_t numBlocksHigh = 0;
        if (height > 0)
        {
            numBlocksHigh = std::max<uint64_t>(1u, (uint64_t(height) + 3u) / 4u);
        }
        rowBytes = numBlocksWide * bpe;
        numRows = numBlocksHigh;
        numBytes = rowBytes * numBlocksHigh;
    }
    else if (packed)
    {
        rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
        numRows = uint64_t(height);
        numBytes = rowBytes * height;
    }
    else if (format == RenderFormat::NV11)
    {
        rowBytes = ((uint64_t(width) + 3u) >> 2) * 4u;
        numRows = uint64_t(height) * 2u; // Direct3D makes this simplifying assumption, although it is larger than the 4:1:1 data
        numBytes = rowBytes * numRows;
    }
    else if (planar)
    {
        rowBytes = ((uint64_t(width) + 1u) >> 1) * bpe;
        numBytes = (rowBytes * uint64_t(height)) + ((rowBytes * uint64_t(height) + 1u) >> 1);
        numRows = height + ((uint64_t(height) + 1u) >> 1);
    }
    else
    {
        size_t bpp = BitsPerPixel(format);
        assert(bpp);

        rowBytes = (uint64_t(width) * bpp + 7u) / 8u; // round up to nearest byte
        numRows = uint64_t(height);
        numBytes = rowBytes * height;
    }

    if (outNumBytes)
    {
        *outNumBytes = static_cast<size_t>(numBytes);
    }
    if (outRowBytes)
    {
        *outRowBytes = static_cast<size_t>(rowBytes);
    }
    if (outNumRows)
    {
        *outNumRows = static_cast<size_t>(numRows);
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