#include "RootSignature.h"

#include "IDArray.h"

#include "Impl/RootSignatureImpl.h"

namespace tpr
{

IDArray<RootSignature_t, RootSignatureDesc> g_RootSignatures;

RootSignature_t CreateRootSignature(const RootSignatureDesc& desc)
{
	RootSignature_t rs = g_RootSignatures.Create(desc);

	if (!RootSignature_CreateImpl(rs, desc))
	{
		g_RootSignatures.Release(rs);
		return RootSignature_t::INVALID;
	}

	return rs;
}

void RenderRef(RootSignature_t rs)
{
	g_RootSignatures.AddRef(rs);
}

void RenderRelease(RootSignature_t rs)
{
	if (g_RootSignatures.Release(rs))
	{
		RootSignature_DestroyImpl(rs);
	}
}

}
