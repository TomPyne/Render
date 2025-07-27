#include "Impl/RootSignatureImpl.h"

#include "RenderImpl.h"
#include "SparseArray.h"

namespace rl
{

SparseArray<ComPtr<ID3D12RootSignature>, RootSignature_t> g_DxRootSignatures;

static D3D12_SHADER_VISIBILITY Dx12_ShaderVisibility(ShaderVisibility Visibility)
{
	switch (Visibility)
	{
	case ShaderVisibility::ALL: return D3D12_SHADER_VISIBILITY_ALL;
	case ShaderVisibility::VERTEX: return D3D12_SHADER_VISIBILITY_VERTEX;
	case ShaderVisibility::GEOMETRY: return D3D12_SHADER_VISIBILITY_GEOMETRY;
	case ShaderVisibility::PIXEL: return D3D12_SHADER_VISIBILITY_PIXEL;
	case ShaderVisibility::AMPLIFICATION: return D3D12_SHADER_VISIBILITY_AMPLIFICATION;
	case ShaderVisibility::MESH: return D3D12_SHADER_VISIBILITY_MESH;
	}
	assert(0 && "Dx12_ShaderVisibility invalid arg");
	return D3D12_SHADER_VISIBILITY_ALL;
}


bool RootSignature_CreateImpl(RootSignature_t rs, const RootSignatureDesc& Desc)
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE FeatureData = {};
	FeatureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(g_render.DxDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &FeatureData, sizeof(FeatureData))))
	{
		OutputDebugStringA("Only root sig version 1.1 supported");
		return false;
	}

	std::vector<D3D12_ROOT_PARAMETER1> RootParams;
	RootParams.resize(Desc.Slots.size());

	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE1>> RangeTable;
	RangeTable.resize(Desc.Slots.size());

	for (uint32_t SlotIt = 0; SlotIt < Desc.Slots.size(); SlotIt++)
	{
		const RootSignatureSlot& SlotDesc = Desc.Slots[SlotIt];

		D3D12_ROOT_PARAMETER1& RootParam = RootParams[SlotIt];
		RootParam.ShaderVisibility = Dx12_ShaderVisibility(SlotDesc.Visibility);

		switch (SlotDesc.Type)
		{
		case RootSignatureSlotType::CONSTANTS:
		{
			RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
			RootParam.Constants.Num32BitValues = SlotDesc.Num32BitVals;
			RootParam.Constants.ShaderRegister = SlotDesc.BaseRegister;
			RootParam.Constants.RegisterSpace = SlotDesc.BaseRegisterSpace;
		}
		break;
		case RootSignatureSlotType::CBV:
		{
			RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
			RootParam.Descriptor.ShaderRegister = SlotDesc.BaseRegister;
			RootParam.Descriptor.RegisterSpace = SlotDesc.BaseRegisterSpace;
			RootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		}
		break;
		case RootSignatureSlotType::SRV:
		{
			RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
			RootParam.Descriptor.ShaderRegister = SlotDesc.BaseRegister;
			RootParam.Descriptor.RegisterSpace = SlotDesc.BaseRegisterSpace;
			RootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		}
		break;
		case RootSignatureSlotType::UAV:
		{
			RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
			RootParam.Descriptor.ShaderRegister = SlotDesc.BaseRegister;
			RootParam.Descriptor.RegisterSpace = SlotDesc.BaseRegisterSpace;
			RootParam.Descriptor.Flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE;
		}
		break;
		case RootSignatureSlotType::DESCRIPTOR_TABLE:
		{
			std::vector<D3D12_DESCRIPTOR_RANGE1>& Ranges = RangeTable[SlotIt];
			Ranges.resize(SlotDesc.RangeCount);
			for (uint32_t RangeIt = 0; RangeIt < SlotDesc.RangeCount; RangeIt++)
			{
				D3D12_DESCRIPTOR_RANGE1& Range = Ranges[RangeIt];
				Range.RangeType = Dx12_DescriptorRangeType(SlotDesc.DescriptorTableType);
				Range.NumDescriptors = UINT_MAX;
				Range.BaseShaderRegister = SlotDesc.BaseRegister;
				Range.RegisterSpace = SlotDesc.BaseRegisterSpace + RangeIt;
				Range.Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
				Range.OffsetInDescriptorsFromTableStart = 0u;
			}

			RootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
			RootParam.DescriptorTable.NumDescriptorRanges = SlotDesc.RangeCount;
			RootParam.DescriptorTable.pDescriptorRanges = RangeTable[SlotIt].data();
		}
		break;
		default:
			DXASSERT(-1); // Invalid slot type
		}
	}

	std::vector<D3D12_STATIC_SAMPLER_DESC> DXStaticSamplers;
	DXStaticSamplers.resize(Desc.GlobalSamplers.size());

	for (uint32_t SampIt = 0; SampIt < Desc.GlobalSamplers.size(); SampIt++)
	{
		const SamplerDesc& SamplerDesc = Desc.GlobalSamplers[SampIt];
		D3D12_STATIC_SAMPLER_DESC& DXSamplerDesc = DXStaticSamplers[SampIt];
		DXSamplerDesc.Filter = Dx12_Filter(SamplerDesc.Comparison != SamplerComparisonFunc::NONE, SamplerDesc.FilterMode.Min, SamplerDesc.FilterMode.Mag, SamplerDesc.FilterMode.Mip);
		DXSamplerDesc.AddressU = Dx12_TextureAddressMode(SamplerDesc.AddressMode.U);
		DXSamplerDesc.AddressV = Dx12_TextureAddressMode(SamplerDesc.AddressMode.V);
		DXSamplerDesc.AddressW = Dx12_TextureAddressMode(SamplerDesc.AddressMode.W);
		DXSamplerDesc.MipLODBias = SamplerDesc.MipLODBias;
		DXSamplerDesc.MaxAnisotropy = SamplerDesc.MaxAnisotropy;
		DXSamplerDesc.ComparisonFunc = Dx12_ComparisonFunc(SamplerDesc.Comparison);
		DXSamplerDesc.BorderColor = Dx12_BorderColor(SamplerDesc.BorderColor);
		DXSamplerDesc.MinLOD = SamplerDesc.MinLOD;
		DXSamplerDesc.MaxLOD = SamplerDesc.MaxLOD;
		DXSamplerDesc.ShaderRegister = SampIt;
		DXSamplerDesc.RegisterSpace = 0u;
		DXSamplerDesc.ShaderVisibility = Dx12_ShaderVisibility(SamplerDesc.Visibility);
	}

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC DXRootSigDesc = {};
	DXRootSigDesc.Version = FeatureData.HighestVersion;

	DXRootSigDesc.Desc_1_1.NumParameters = (UINT)RootParams.size();
	DXRootSigDesc.Desc_1_1.pParameters = RootParams.data();
	DXRootSigDesc.Desc_1_1.NumStaticSamplers = (UINT)DXStaticSamplers.size();
	DXRootSigDesc.Desc_1_1.pStaticSamplers = DXStaticSamplers.data();

	DXRootSigDesc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	if ((Desc.Flags & RootSignatureFlags::ALLOW_INPUT_LAYOUT) != RootSignatureFlags::NONE)
	{
		DXRootSigDesc.Desc_1_1.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	}

	ComPtr<ID3DBlob> DXSerializedRootSig;
	ComPtr<ID3DBlob> ErrorBlob;
	if (!DXENSURE(D3D12SerializeVersionedRootSignature(&DXRootSigDesc, &DXSerializedRootSig, &ErrorBlob)))
	{
		if (ErrorBlob)
		{
			OutputDebugStringA((LPCSTR)ErrorBlob->GetBufferPointer());
		}
		return false;
	}

	ComPtr<ID3D12RootSignature>& DXRootSig = g_DxRootSignatures.Alloc(rs);

	if (!DXENSURE(g_render.DxDevice->CreateRootSignature(0u, DXSerializedRootSig->GetBufferPointer(), DXSerializedRootSig->GetBufferSize(), IID_PPV_ARGS(&DXRootSig))))
	{
		return false;
	}

	return true;
}

ID3D12RootSignature* Dx12_GetRootSignature(RootSignature_t rs)
{
	if (g_DxRootSignatures.Valid(rs))
	{
		return g_DxRootSignatures[rs].Get();
	}

	return nullptr;
}

void RootSignature_DestroyImpl(RootSignature_t rs)
{
	g_DxRootSignatures.Free(rs);
}

}