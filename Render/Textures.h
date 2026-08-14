#pragma once

#include "RenderTypes.h"

namespace rl
{

enum class TextureDimension : uint8_t
{
	UNKNOWN,
	TEX1D,
	TEX1D_ARRAY,
	TEX2D,
	TEX2D_ARRAY,
	CUBEMAP,
	CUBEMAP_ARRAY,
	TEX3D,
};

struct MipData
{
	size_t RowPitch = 0;
	size_t SlicePitch = 0;
	const void* Data = nullptr;

	MipData() = default;
	MipData(const void* data, RenderFormat format, uint32_t width, uint32_t height);
};

struct TextureCreateDesc
{
	uint32_t Width				= 0;
	uint32_t Height				= 0;
	RenderFormat Format			= RenderFormat::UNKNOWN;
	RenderResourceFlags Flags	= RenderResourceFlags::NONE;
	MipData* Data				= nullptr;

	std::wstring DebugName;
};

enum class TextureCPUAccess : uint8_t
{
	NONE,
	READ = (1u << 0u),
	WRITE = (1u << 1u),
};
IMPLEMENT_FLAGS(TextureCPUAccess, uint8_t);

struct TextureCreateDescEx
{
	uint32_t Width							= 0u;
	uint32_t Height							= 0u;
	uint32_t DepthOrArraySize				= 1u;
	uint32_t MipCount						= 1u;
	TextureDimension Dimension				= TextureDimension::UNKNOWN;
	RenderResourceFlags Flags				= RenderResourceFlags::NONE;	
	ResourceUsage Usage						= ResourceUsage::DEFAULT;
	TextureCPUAccess CpuAccess				= TextureCPUAccess::NONE;
	const MipData* Data						= nullptr;	
	ResourceTransitionState InitialState	= ResourceTransitionState::COMMON;
	RenderFormat ResourceFormat				= RenderFormat::UNKNOWN;

	std::wstring DebugName;

	TextureCreateDescEx() = default;
	TextureCreateDescEx(const TextureCreateDesc& Desc);
};

Texture_t CreateTexture(const void* const data, RenderFormat format, uint32_t width, uint32_t height);
Texture_t CreateTexture(const TextureCreateDesc& desc);
Texture_t CreateTextureEx(const TextureCreateDescEx& desc);

Texture_t AllocTexture();

const TextureCreateDescEx* GetTextureDesc(Texture_t tex);

// The params here are for validation to ensure we are copying the intended data.
void UpdateTexture(Texture_t tex, const void* const data, uint32_t width, uint32_t height, RenderFormat format);

void GetTextureDims(Texture_t tex, uint32_t* w, uint32_t* h);

enum class TextureResourceAccessMethod : uint32_t
{
	Read,
	Write,
	ReadWrite,
};

struct TextureResourceAccessScope
{
	void* Ptr = nullptr;

	size_t RowPitch = 0;
	size_t DepthPitch = 0;

	Texture_t MappedTex = Texture_t::INVALID;
	uint32_t SubResIdx = 0;

	explicit TextureResourceAccessScope(Texture_t resource, TextureResourceAccessMethod method, uint32_t subResourceIndex);
	~TextureResourceAccessScope();
};

}