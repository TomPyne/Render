#include "DxCompiler.h"

#include "Impl/Dx/DxErrorHandling.h"

#include <cstdio>
#include <dxcapi.h>

#ifdef _MSC_VER
#pragma comment(lib, "dxcompiler.lib")
#endif

#define DXC_DEBUG_SHADERS 1

namespace rl
{

LPCWSTR ShaderProfileStr[(uint8_t)ShaderProfile::COUNT] =
{
	L"cs_5_0",
	L"ps_5_0",
	L"vs_5_0",
	L"gs_5_0",
	L"cs_5_1",
	L"ps_5_1",
	L"vs_5_1",
	L"gs_5_1",
	L"cs_6_0",
	L"ps_6_0",
	L"vs_6_0",
	L"gs_6_0",
	L"ms_6_5",
	L"as_6_5",
	L"lib_6_3",
};

static std::wstring ToWideStr(const std::string& str)
{
	if (str.empty())
		return std::wstring{};

	size_t len = ::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), 0, 0);

	std::wstring wstr = std::wstring(len, L'\0');

	::MultiByteToWideChar(CP_UTF8, 0, str.c_str(), (int)str.length(), wstr.data(), (int)wstr.length());

	return wstr;
}

static std::string LoadShaderCode(const std::string& filePath)
{
	FILE* fp = nullptr;

	if (!DXENSURE(fopen_s(&fp, filePath.c_str(), "rb")))
	{
		return std::string{};
	}

	std::string ret;

	if (fp)
	{
		fseek(fp, 0, SEEK_END);
		size_t fileSize = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		ret.resize(fileSize);
		fread(ret.data(), fileSize, 1, fp);
		fclose(fp);
	}

	return ret;
}

ComPtr<IDxcResult> CompileShader(const std::string& shaderCode, const char* includeDirectory, ShaderProfile profile, const ShaderMacros& macros)
{	
	ComPtr<IDxcUtils> utils;
	if (!DXENSURE(DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&utils))))
		return nullptr;

	ComPtr<IDxcIncludeHandler> includeHandler;
	if (!DXENSURE(utils->CreateDefaultIncludeHandler(&includeHandler)))
		return nullptr;

	ComPtr<IDxcCompiler3> compiler3;
	if (!DXENSURE(DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler3))))
		return nullptr;

	ComPtr<IDxcBlobEncoding> source;
	if (!DXENSURE(utils->CreateBlob(shaderCode.c_str(), (UINT32)shaderCode.length(), CP_UTF8, &source)))
		return nullptr;

	std::vector<LPCWSTR> arguments;

	// Entry point
	if (profile != ShaderProfile::LIB_6_3)
	{
		arguments.push_back(L"-E");
		arguments.push_back(L"main");
	}

	// Shader profile
	arguments.push_back(L"-T");
	arguments.push_back(ShaderProfileStr[(uint8_t)profile]);

	wchar_t wPath[MAX_PATH];
	size_t len;
	if (includeDirectory && strlen(includeDirectory) < MAX_PATH)
	{
		mbstowcs_s(&len, wPath, includeDirectory, strlen(includeDirectory));

		// Include directory
		arguments.push_back(L"-I");
		arguments.push_back(wPath);
	}

	arguments.push_back(DXC_ARG_WARNINGS_ARE_ERRORS); //-WX

#if DXC_DEBUG_SHADERS
	arguments.push_back(DXC_ARG_DEBUG); //-Zi
	arguments.push_back(DXC_ARG_SKIP_OPTIMIZATIONS); //-Od
#else
	// Strip out debug and reflection data
	arguments.push_back(L"-Qstrip_debug");
	arguments.push_back(L"-Qstrip_reflect");
#endif

	std::vector<std::wstring> defines;
	defines.resize(macros.size());
	for (uint32_t i = 0; i < defines.size(); i++)
	{
		defines[i] = ToWideStr(macros[i]._define + "=" + macros[i]._value);

		arguments.push_back(L"-D");
		arguments.push_back(defines[i].c_str());
	}

	DxcBuffer sourceBuffer;
	sourceBuffer.Ptr = source->GetBufferPointer();
	sourceBuffer.Size = source->GetBufferSize();
	sourceBuffer.Encoding = 0;

	ComPtr<IDxcResult> result;
	if (!DXENSURE(compiler3->Compile(&sourceBuffer, arguments.data(), (UINT32)arguments.size(), includeHandler.Get(), IID_PPV_ARGS(&result))))
		return nullptr;

	ComPtr<IDxcBlobUtf8> errors;
	ComPtr<IDxcBlobUtf16> outputName;
	if (DXENSURE(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), &outputName)))
	{
		if (errors && errors->GetStringLength() > 0)
		{
			OutputDebugStringA((LPCSTR)errors->GetBufferPointer());

			return nullptr;
		}
	}

	return result;
}

ComPtr<IDxcResult> CompileShaderFromFile(const std::string& path, const char* includeDirectory, ShaderProfile profile, const ShaderMacros& macros)
{
	std::string shaderCode = LoadShaderCode(path);
	if (shaderCode.empty())
	{
		OutputDebugStringA("CompileShaderFromFile: failed to load shader code");
		return nullptr;
	}	

	return CompileShader(shaderCode, includeDirectory, profile, macros);
}

}
