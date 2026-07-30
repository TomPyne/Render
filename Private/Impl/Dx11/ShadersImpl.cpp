#include "Impl/ShadersImpl.h"

#include "RenderTypes.h"
#include "RenderImpl.h"

#include <d3dcompiler.h>

#ifdef _MSC_VER
#pragma comment(lib, "d3dcompiler.lib") // Automatically link with d3dcompiler.lib as we are using D3DCompile() below.
#endif

#define CS_PROFILE "cs_5_0"
#define PS_PROFILE "ps_5_0"
#define VS_PROFILE "vs_5_0"
#define GS_PROFILE "gs_5_0"

namespace rl
{

std::vector<ComPtr<ID3DBlob>>				g_vertexShaderBlobs; // Hold these for input layout creation;
std::vector<ComPtr<ID3D11VertexShader>>		g_vertexShaders;
std::vector<ComPtr<ID3D11PixelShader>>		g_pixelShaders;
std::vector<ComPtr<ID3D11GeometryShader>>	g_geometryShaders;
std::vector<ComPtr<ID3D11ComputeShader>>	g_computeShaders;

static ComPtr<ID3DBlob>& AllocVertexBlob(VertexShader_t vs)
{
	if ((size_t)vs >= g_vertexShaderBlobs.size())
		g_vertexShaderBlobs.resize((size_t)vs + 1);

	return g_vertexShaderBlobs[(size_t)vs];
}

static ComPtr<ID3D11VertexShader>& AllocVs(VertexShader_t vs)
{
	if ((size_t)vs >= g_vertexShaders.size())
		g_vertexShaders.resize((size_t)vs + 1);

	return g_vertexShaders[(size_t)vs];
}

static ComPtr<ID3D11PixelShader>& AllocPs(PixelShader_t ps)
{
	if ((size_t)ps >= g_pixelShaders.size())
		g_pixelShaders.resize((size_t)ps + 1);

	return g_pixelShaders[(size_t)ps];
}

static ComPtr<ID3D11GeometryShader>& AllocGs(GeometryShader_t gs)
{
	if ((size_t)gs >= g_geometryShaders.size())
		g_geometryShaders.resize((size_t)gs + 1);

	return g_geometryShaders[(size_t)gs];
}

static ComPtr<ID3D11ComputeShader>& AllocCs(ComputeShader_t cs)
{
	if ((size_t)cs >= g_computeShaders.size())
		g_computeShaders.resize((size_t)cs + 1);

	return g_computeShaders[(size_t)cs];
}

ShaderCompileResult CompileShaderInternal(const char* target, const ShaderCompileParams Params, ComPtr<ID3DBlob>& shaderBlob)
{
	const size_t numMacros = Params.Macros.size();
	std::vector<D3D_SHADER_MACRO> dxMacros;
	dxMacros.resize(numMacros + 1);

	D3D_SHADER_MACRO* pdxMacros = nullptr;
	if (numMacros > 0)
	{
		for (size_t defIt = 0; defIt < numMacros; defIt++)
			dxMacros[defIt] = D3D_SHADER_MACRO({ Params.Macros[defIt]._define.c_str(), Params.Macros[defIt]._value.c_str() });

		dxMacros[numMacros] = D3D_SHADER_MACRO({ NULL, NULL });

		pdxMacros = dxMacros.data();
	}

	size_t len = strlen(Params.Path);

	assert(len < MAX_PATH && "CompileShader path length exceeds MAX_PATH");

	wchar_t wPath[MAX_PATH];

	mbstowcs_s(&len, wPath, Params.Path, len);

	ComPtr<ID3DBlob> errBlob;

	HRESULT hr = D3DCompileFromFile(
		wPath,
		pdxMacros,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		target,
		0,
		0,
		&shaderBlob,
		&errBlob
	);

	if (FAILED(hr))
	{
		fprintf(stderr, "Error compiling '%s' : \n\n", Params.Path);
		if (errBlob)
		{
			return (char*)errBlob->GetBufferPointer();
		}

		return false;
	}

    return true;
}

ShaderCompileResult CompileShader(VertexShader_t handle, const ShaderCompileParams Params)
{
	AllocVertexBlob(handle);

	auto& blob = g_vertexShaderBlobs[(uint32_t)handle];

	if (!CompileShaderInternal(VS_PROFILE, Params, blob))
		return false;

	auto& dxVs = AllocVs(handle);

	return SUCCEEDED(g_render.Device->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &dxVs));
}

ShaderCompileResult CompileShader(PixelShader_t handle, const ShaderCompileParams Params)
{
	ComPtr<ID3DBlob> shaderBlob;
	if (!CompileShaderInternal(PS_PROFILE, Params, shaderBlob))
		return false;

	auto& dxPs = AllocPs(handle);

	return SUCCEEDED(g_render.Device->CreatePixelShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &dxPs));
}

ShaderCompileResult CompileShader(GeometryShader_t handle, const ShaderCompileParams Params)
{
	ComPtr<ID3DBlob> shaderBlob;
	if(!CompileShaderInternal(GS_PROFILE, Params, shaderBlob))
		return false;

	auto& dxGs = AllocGs(handle);

	return SUCCEEDED(g_render.Device->CreateGeometryShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &dxGs));
}

ShaderCompileResult CompileShader(MeshShader_t handle, const ShaderCompileParams Params)
{
	assert(0 && "Unsupported");
	return false;
}

ShaderCompileResult CompileShader(AmplificationShader_t handle, const ShaderCompileParams Params)
{
	assert(0 && "Unsupported");
	return false;
}

ShaderCompileResult CompileShader(ComputeShader_t handle, const ShaderCompileParams Params)
{
	ComPtr<ID3DBlob> shaderBlob;

	if (!CompileShaderInternal(CS_PROFILE, Params, shaderBlob))
		return false;

	auto& dxCs = AllocCs(handle);

	return SUCCEEDED(g_render.Device->CreateComputeShader(shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize(), nullptr, &dxCs));
}

ShaderCompileResult CompileShader(RaytracingRayGenShader_t handle, const ShaderCompileParams Params)
{
	assert(0 && "Unsupported");
	return false;
}

ShaderCompileResult CompileShader(RaytracingMissShader_t handle, const ShaderCompileParams Params)
{
	assert(0 && "Unsupported");
	return false;
}

ShaderCompileResult CompileShader(RaytracingAnyHitShader_t handle, const ShaderCompileParams Params)
{
	assert(0 && "Unsupported");
	return false;
}

ShaderCompileResult CompileShader(RaytracingClosestHitShader_t handle, const ShaderCompileParams Params)
{
	assert(0 && "Unsupported");
	return false;
}

ID3DBlob* Dx11_GetVertexShaderBlob(VertexShader_t handle)
{
	return (size_t)handle > 0 && (size_t)handle < g_vertexShaderBlobs.size() ? g_vertexShaderBlobs[(size_t)handle].Get() : nullptr;
}

ID3D11VertexShader* Dx11_GetVertexShader(VertexShader_t handle)
{
	return (size_t)handle > 0 && (size_t)handle < g_vertexShaders.size() ? g_vertexShaders[(size_t)handle].Get() : nullptr;
}

ID3D11PixelShader* Dx11_GetPixelShader(PixelShader_t handle)
{
	return (size_t)handle > 0 && (size_t)handle < g_pixelShaders.size() ? g_pixelShaders[(size_t)handle].Get() : nullptr;
}

ID3D11GeometryShader* Dx11_GetGeometryShader(GeometryShader_t handle)
{
	return (size_t)handle > 0 && (size_t)handle < g_geometryShaders.size() ? g_geometryShaders[(size_t)handle].Get() : nullptr;
}

ID3D11ComputeShader* Dx11_GetComputeShader(ComputeShader_t handle)
{
	return (size_t)handle > 0 && (size_t)handle < g_computeShaders.size() ? g_computeShaders[(size_t)handle].Get() : nullptr;
}

}