#include "Shaders.h"

#include "IDArray.h"

#include "Impl/ShadersImpl.h"

#include "Render.h"

#include <algorithm>
#include <windows.h>

namespace tpr
{

ShaderMacro::ShaderMacro(const char* define, uint32_t value)
	: _define(define)
{
	_value = std::to_string(value);
}

struct ShaderData
{
	bool Compiled = false;
	std::string Path;
	ShaderMacros Macros;

	std::string ShaderIDStr;
};

IDArray<VertexShader_t,			ShaderData>	g_VertexShaders;
IDArray<PixelShader_t,			ShaderData>	g_PixelShaders;
IDArray<GeometryShader_t,		ShaderData>	g_GeometryShaders;
IDArray<MeshShader_t,			ShaderData>	g_MeshShaders;
IDArray<AmplificationShader_t,	ShaderData>	g_AmplificationShaders;
IDArray<ComputeShader_t,		ShaderData>	g_ComputeShaders;

std::string CreateShaderIDStr(const char* path, const ShaderMacros& macros)
{
	std::vector<std::string> sortedMacros;
	for (const ShaderMacro& macro : macros)
	{
		sortedMacros.push_back(macro._define + macro._value);
	}

	std::sort(sortedMacros.begin(), sortedMacros.end());

	std::string idStr = path;
	for (const std::string& macro : sortedMacros)
	{
		idStr += macro;
	}

	std::transform(idStr.begin(), idStr.end(), idStr.begin(), [](unsigned char c) {return std::tolower(c); });

	return idStr;
}

template<typename T>
T FindShader(IDArray<T, ShaderData>& loaded, const std::string& shaderIDStr)
{
	T found = T::INVALID;

	loaded.ForEachValid([&](T handle, const ShaderData& data)
	{
		if (shaderIDStr == data.ShaderIDStr)
		{
			found = handle;
		}

		return found == T::INVALID;
	});

	return found;
}

static void AppendShaderPlatformMacros(ShaderMacros* macros)
{
	macros->push_back({ Render_ApiId(), "1" });

	if (Render_IsBindless())
	{
		macros->push_back({ "_BINDLESS", "1" });
		macros->push_back({ "_BINDLESS_MAX", "1" });
	}
}

static void UpdateShader(ShaderData* data, const char* path, const ShaderMacros& macros, const std::string& shaderIdStr)
{
	if (!data)
		return;

	data->Path = path;
	data->Macros = macros;
	data->ShaderIDStr = shaderIdStr;
	data->Compiled = true;
}

template<typename ShaderHandle>
ShaderHandle CreateShader(const char* path, const ShaderMacros& macros, const char* shaderTypeMacro, IDArray<ShaderHandle, ShaderData>& shaderArray)
{
	ShaderMacros fullMacros = macros;
	fullMacros.push_back({ shaderTypeMacro, "1" });
	AppendShaderPlatformMacros(&fullMacros);

	std::string shaderIdStr = CreateShaderIDStr(path, fullMacros);

	ShaderHandle handle = FindShader(shaderArray, shaderIdStr);

	if (handle == ShaderHandle::INVALID)
	{
		handle = shaderArray.Create();

		{
			// TODO Multithread: Would be nicer if we could forward the constructor values to the shader in create I think to avoid this extra lock
			auto lock = shaderArray.ReadScopeLock();

			ShaderData* data = shaderArray.Get(handle);

			UpdateShader(data, path, fullMacros, shaderIdStr);
		}

		// TODO: Move this directory stripping code to a shared place. Make less platform specific
		std::string directory = {};
		{
			std::string pathStr = path;
			size_t lastSlash = pathStr.find_last_of('/');
			if (lastSlash != std::string::npos)
			{
				directory = pathStr.substr(0, lastSlash);
				DWORD ftyp = GetFileAttributesA(directory.c_str());
				if (ftyp == INVALID_FILE_ATTRIBUTES || (ftyp & FILE_ATTRIBUTE_DIRECTORY) == 0)
				{
					directory = {};
				}
			}
		}

		// TODO: Make dialog box less platform specific
		while (!CompileShader(handle, path, directory.empty() ? nullptr : directory.c_str(), fullMacros))
		{
			std::string message = "Failed to compile " + std::string(path) + ", retry?";
			int input = MessageBoxA(NULL, message.c_str(), "Shader compilation error", MB_RETRYCANCEL | MB_ICONERROR);

			if (input != IDRETRY)
			{
				shaderArray.Release(handle);
				return ShaderHandle::INVALID;
				break;
			}
		}
	}

	return handle;
}

VertexShader_t CreateVertexShader(const char* path, const ShaderMacros& macros)
{
	return CreateShader(path, macros, "_VS", g_VertexShaders);
}

PixelShader_t CreatePixelShader(const char* path, const ShaderMacros& macros)
{
	return CreateShader(path, macros, "_PS", g_PixelShaders);
}

GeometryShader_t CreateGeometryShader(const char* path, const ShaderMacros& macros)
{
	return CreateShader(path, macros, "_GS", g_GeometryShaders);
}

MeshShader_t CreateMeshShader(const char* path, const ShaderMacros& macros)
{
	if (!Render_SupportsMeshShaders())
		return {};

	return CreateShader(path, macros, "_MS", g_MeshShaders);
}

AmplificationShader_t CreateAmplificationShader(const char* path, const ShaderMacros& macros)
{
	if (!Render_SupportsMeshShaders())
		return {};

	return CreateShader(path, macros, "_AS", g_AmplificationShaders);
}

ComputeShader_t CreateComputeShader(const char* path, const ShaderMacros& macros)
{
	return CreateShader(path, macros, "_CS", g_ComputeShaders);
}

size_t GetVertexShaderCount()
{
	return g_VertexShaders.UsedSize();
}

size_t GetPixelShaderCount()
{
	return g_PixelShaders.UsedSize();
}

size_t GetGeometryShaderCount()
{
	return g_GeometryShaders.UsedSize();
}

size_t GetMeshShaderCount()
{
	return g_MeshShaders.UsedSize();
}

size_t GetAmplificationShaderCount()
{
	return g_AmplificationShaders.UsedSize();
}

size_t GetComputeShaderCount()
{
	return g_ComputeShaders.UsedSize();
}

template<typename ShaderHandle>
void ReloadShaderType(IDArray<ShaderHandle, ShaderData>& shaderArray)
{
	shaderArray.ForEachValid([] (ShaderHandle handle, const ShaderData& data)
	{
		if (data.Compiled)
		{
			CompileShader(handle, data.Path.c_str(), nullptr, data.Macros);
		}

		return true;
	});
}

void ReloadShaders()
{
	ReloadShaderType(g_VertexShaders);
	ReloadShaderType(g_PixelShaders);
	ReloadShaderType(g_GeometryShaders);
	ReloadShaderType(g_MeshShaders);
	ReloadShaderType(g_AmplificationShaders);
	ReloadShaderType(g_ComputeShaders);
}

// Don't bother ref counting or releasing shaders, we always keep them in memory as they are expensive to re-initialize
void RenderRef(VertexShader_t vs)
{
}

void RenderRef(PixelShader_t ps)
{
}

void RenderRef(GeometryShader_t gs)
{
}

void RenderRef(MeshShader_t ms)
{
}

void RenderRef(AmplificationShader_t as)
{
}

void RenderRef(ComputeShader_t cs)
{
}

void RenderRelease(VertexShader_t vs)
{		
}

void RenderRelease(PixelShader_t ps)
{
}

void RenderRelease(GeometryShader_t gs)
{
}

void RenderRelease(ComputeShader_t cs)
{
}


}
