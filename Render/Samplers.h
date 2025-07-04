#pragma once

#include "RenderTypes.h"

namespace rl
{

enum class SamplerAddressMode : uint8_t
{
	WRAP,
	MIRROR,
	CLAMP,
	BORDER,
	MIRROR_ONCE
};

enum class SamplerFilterMode : uint8_t
{
	POINT,
	LINEAR,
	ANISOTROPIC,
};

enum class SamplerComparisonFunc : uint8_t
{
	NONE,
	NEVER,
	LESS,
	EQUAL,
	LESS_EQUAL,
	GREATER,
	NOT_EQUAL,
	GREATER_EQUAL,
	ALWAYS,
};

enum class SamplerBorderColor : uint8_t
{
	TRANSPARENT_BLACK,
	OPAQUE_BLACK,
	OPAQUE_WHITE,
};

struct SamplerDesc
{
	SamplerDesc() = default;

	union
	{
		struct
		{
			SamplerAddressMode U;
			SamplerAddressMode V;
			SamplerAddressMode W;
		} AddressMode;
		uint32_t Opaque = 0;
	};

	union
	{
		struct
		{
			SamplerFilterMode Min;
			SamplerFilterMode Mag;
			SamplerFilterMode Mip;
		} FilterMode;
		uint32_t Opaque = 0;
	};

	SamplerComparisonFunc Comparison = SamplerComparisonFunc::NONE;
	float MinLOD = 0.0f;
	float MaxLOD = FLT_MAX;
	float MipLODBias = 0.0f;
	SamplerBorderColor BorderColor = SamplerBorderColor::TRANSPARENT_BLACK;
	uint32_t MaxAnisotropy = 16;

	ShaderVisibility Visibility = ShaderVisibility::ALL;

	inline SamplerDesc& AddressModeUVW(SamplerAddressMode am) noexcept
	{
		AddressMode.U = AddressMode.V = AddressMode.W = am; return *this;		
	}

	inline SamplerDesc& FilterModeMinMagMip(SamplerFilterMode fm) noexcept
	{
		FilterMode.Min = FilterMode.Mag = FilterMode.Mip = fm; return *this;
	}

	inline SamplerDesc& ComparisonFunc(SamplerComparisonFunc cf) noexcept
	{
		Comparison = cf; return *this;
	}

	inline SamplerDesc& BorderCol(SamplerBorderColor col) noexcept
	{
		BorderColor = col; return *this;
	}

	inline SamplerDesc& ShaderVisibility(ShaderVisibility InVisibility) noexcept
	{
		Visibility = InVisibility; return *this;
	}
};

}