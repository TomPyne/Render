#include "Impl/ShadersImpl.h"

#include "Impl/Dxc/DxCompiler.h"
#include "RenderTypes.h"
#include "RenderImpl.h"
#include "SparseArray.h"

#include <dxcapi.h>

namespace rl
{

struct
{
	SparseArray<ComPtr<IDxcBlob>, VertexShader_t>				CompiledVertexBlobs;
	SparseArray<ComPtr<IDxcBlob>, PixelShader_t>				CompiledPixelBlobs;
	SparseArray<ComPtr<IDxcBlob>, GeometryShader_t>				CompiledGeometryBlobs;
	SparseArray<ComPtr<IDxcBlob>, MeshShader_t>					CompiledMeshBlobs;
	SparseArray<ComPtr<IDxcBlob>, AmplificationShader_t>		CompiledAmplificationBlobs;
	SparseArray<ComPtr<IDxcBlob>, ComputeShader_t>				CompiledComputeBlobs;
	SparseArray<ComPtr<IDxcBlob>, RaytracingRayGenShader_t>		CompiledRayGenBlobs;
	SparseArray<ComPtr<IDxcBlob>, RaytracingMissShader_t>		CompiledRayMissBlobs;
	SparseArray<ComPtr<IDxcBlob>, RaytracingAnyHitShader_t>		CompiledRayAnyHitBlobs;
	SparseArray<ComPtr<IDxcBlob>, RaytracingClosestHitShader_t>	CompiledRayClosestHitBlobs;
} g_shaders;

ShaderCompileResult CompileShaderInternal(ShaderProfile target, const ShaderCompileParams Params, ComPtr<IDxcBlob>& shaderBlob)
{	
	std::string ErrorMsg;
	ComPtr<IDxcResult> result = CompileShaderFromFile(Params.Path, Params.Directory, target, Params.Macros, ErrorMsg);

	if (!result)
	{
		return ErrorMsg.c_str();
	}

	if (!result->HasOutput(DXC_OUT_OBJECT))
	{
		return "CompileShader failed - result->HasOutput(DXC_OUT_OBJECT)\n";
	}		

	ComPtr<IDxcBlobUtf16> outputName;
	if (!DXENSURE(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), &outputName)))
		return false;

	return true;
}

ShaderCompileResult CompileShader(VertexShader_t handle, const ShaderCompileParams Params)
{
	return CompileShaderInternal(ShaderProfile::VS_6_0, Params, g_shaders.CompiledVertexBlobs.Alloc(handle));
}

ShaderCompileResult CompileShader(PixelShader_t handle, const ShaderCompileParams Params)
{
	return CompileShaderInternal(ShaderProfile::PS_6_0, Params, g_shaders.CompiledPixelBlobs.Alloc(handle));
}

ShaderCompileResult CompileShader(GeometryShader_t handle, const ShaderCompileParams Params)
{
	return CompileShaderInternal(ShaderProfile::GS_6_0, Params, g_shaders.CompiledGeometryBlobs.Alloc(handle));
}

ShaderCompileResult CompileShader(MeshShader_t handle, const ShaderCompileParams Params)
{
	return CompileShaderInternal(ShaderProfile::MS_6_0, Params, g_shaders.CompiledMeshBlobs.Alloc(handle));
}

ShaderCompileResult CompileShader(AmplificationShader_t handle, const ShaderCompileParams Params)
{
	return CompileShaderInternal(ShaderProfile::AS_6_0, Params, g_shaders.CompiledAmplificationBlobs.Alloc(handle));
}

ShaderCompileResult CompileShader(ComputeShader_t handle, const ShaderCompileParams Params)
{
	return CompileShaderInternal(ShaderProfile::CS_6_0, Params, g_shaders.CompiledComputeBlobs.Alloc(handle));
}

ShaderCompileResult CompileShader(RaytracingRayGenShader_t handle, const ShaderCompileParams Params)
{
	return CompileShaderInternal(ShaderProfile::LIB_6_3, Params, g_shaders.CompiledRayGenBlobs.Alloc(handle));
}

ShaderCompileResult CompileShader(RaytracingMissShader_t handle, const ShaderCompileParams Params)
{
	return CompileShaderInternal(ShaderProfile::LIB_6_3, Params, g_shaders.CompiledRayMissBlobs.Alloc(handle));
}

ShaderCompileResult CompileShader(RaytracingAnyHitShader_t handle, const ShaderCompileParams Params)
{
	return CompileShaderInternal(ShaderProfile::LIB_6_3, Params, g_shaders.CompiledRayAnyHitBlobs.Alloc(handle));
}

ShaderCompileResult CompileShader(RaytracingClosestHitShader_t handle, const ShaderCompileParams Params)
{
	return CompileShaderInternal(ShaderProfile::LIB_6_3, Params, g_shaders.CompiledRayClosestHitBlobs.Alloc(handle));
}

IDxcBlob* Dx12_GetVertexShaderBlob(VertexShader_t vs)
{
	return g_shaders.CompiledVertexBlobs[vs].Get();
}

IDxcBlob* Dx12_GetPixelShaderBlob(PixelShader_t ps)
{
	return g_shaders.CompiledPixelBlobs[ps].Get();
}

IDxcBlob* Dx12_GetGeometryShaderBlob(GeometryShader_t gs)
{
	return g_shaders.CompiledGeometryBlobs[gs].Get();
}

IDxcBlob* Dx12_GetMeshShaderBlob(MeshShader_t ms)
{
	return g_shaders.CompiledMeshBlobs[ms].Get();
}

IDxcBlob* Dx12_GetAmplificationShaderBlob(AmplificationShader_t as)
{
	return g_shaders.CompiledAmplificationBlobs[as].Get();
}

IDxcBlob* Dx12_GetComputeShaderBlob(ComputeShader_t cs)
{
	return g_shaders.CompiledComputeBlobs[cs].Get();
}

IDxcBlob* Dx12_GetRayGenShaderBlob(RaytracingRayGenShader_t rgs)
{
	return g_shaders.CompiledRayGenBlobs[rgs].Get();
}

IDxcBlob* Dx12_GetRayMissShaderBlob(RaytracingMissShader_t rms)
{
	return g_shaders.CompiledRayMissBlobs[rms].Get();
}

IDxcBlob* Dx12_GetRayAnyHitShaderBlob(RaytracingAnyHitShader_t ras)
{
	return g_shaders.CompiledRayAnyHitBlobs[ras].Get();
}

IDxcBlob* Dx12_GetRayClosestHitShaderBlob(RaytracingClosestHitShader_t rcs)
{
	return g_shaders.CompiledRayClosestHitBlobs[rcs].Get();
}

}
