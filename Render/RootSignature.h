#pragma once

#include "RenderTypes.h"
#include "Samplers.h"

namespace rl
{

enum class RootSignatureFlags : uint32_t
{
	NONE = 0u,
	ALLOW_INPUT_LAYOUT = (1u << 0u),
};
IMPLEMENT_FLAGS(RootSignatureFlags, uint32_t);

enum class RootSignatureSlotType : uint32_t
{
	NONE,
	CONSTANTS,
	CBV,
	SRV,
	UAV,
	DESCRIPTOR_TABLE,
};

enum class RootSignatureDescriptorTableType : uint32_t
{
	NONE,
	SRV,
	UAV,
};

struct RootSignatureSlot
{
	RootSignatureSlotType Type = RootSignatureSlotType::NONE;

	uint32_t BaseRegister = 0;
	uint32_t BaseRegisterSpace = 0;

	// Descriptor Table Settings
	RootSignatureDescriptorTableType DescriptorTableType = RootSignatureDescriptorTableType::NONE;
	uint32_t RangeCount;

	// Constants Settings
	uint32_t Num32BitVals;

	ShaderVisibility Visibility = ShaderVisibility::ALL;

	static constexpr RootSignatureSlot MakeSlot(RootSignatureSlotType Type, uint32_t BaseRegister, ShaderVisibility Visibility = ShaderVisibility::ALL)
	{
		RootSignatureSlot Slot = {};
		Slot.Type = Type;
		Slot.BaseRegister = BaseRegister;
		Slot.Visibility = Visibility;
		return Slot;
	}

	static constexpr RootSignatureSlot ConstantsSlot(uint32_t Num32BitVals, uint32_t BaseRegister, ShaderVisibility Visibility = ShaderVisibility::ALL)
	{
		RootSignatureSlot Slot = MakeSlot(RootSignatureSlotType::CONSTANTS, BaseRegister, Visibility);
		Slot.Num32BitVals = Num32BitVals;
		return Slot;
	}

	static constexpr RootSignatureSlot CBVSlot(uint32_t BaseRegister, uint32_t Space, ShaderVisibility Visibility = ShaderVisibility::ALL)
	{
		RootSignatureSlot Slot = MakeSlot(RootSignatureSlotType::CBV, BaseRegister, Visibility);
		Slot.BaseRegisterSpace = Space;
		return Slot;
	}

	static constexpr RootSignatureSlot SRVSlot(uint32_t BaseRegister, uint32_t Space, ShaderVisibility Visibility = ShaderVisibility::ALL)
	{
		RootSignatureSlot Slot = MakeSlot(RootSignatureSlotType::SRV, BaseRegister, Visibility);
		Slot.BaseRegisterSpace = Space;
		return Slot;
	}

	static constexpr RootSignatureSlot DescriptorTableSlot(uint32_t BaseRegister, uint32_t Space, RootSignatureDescriptorTableType TableType, uint32_t RangeCount = (uint32_t)0xffff, ShaderVisibility Visibility = ShaderVisibility::ALL)
	{
		RootSignatureSlot Slot = MakeSlot(RootSignatureSlotType::DESCRIPTOR_TABLE, BaseRegister, Visibility);
		Slot.DescriptorTableType = TableType;
		Slot.RangeCount = RangeCount;
		Slot.BaseRegisterSpace = Space;
		return Slot;
	}
};

struct RootSignatureDesc
{
	RootSignatureFlags Flags = RootSignatureFlags::NONE;
	std::vector<RootSignatureSlot> Slots;
	std::vector<SamplerDesc> GlobalSamplers;
};

RootSignature_t CreateRootSignature(const RootSignatureDesc& Desc);

}