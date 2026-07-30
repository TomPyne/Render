#pragma once

#include "Shaders.h"

#include <string>

namespace rl
{

struct ShaderCompileParams
{
	ShaderCompileParams(const char* InPath, const char* InDirectory, const ShaderMacros& InMacros)
		: Path(InPath)
		, Directory(InDirectory)
		, Macros(InMacros)
	{}
	const char* Path;
	const char* Directory;
	const ShaderMacros& Macros;
};

struct ShaderCompileResult
{
	ShaderCompileResult(bool InSuccess)
		: Success(InSuccess)
	{}

	ShaderCompileResult(const char* InMessage)
		: Success(false)
		, ErrorMessage(InMessage)
	{}

	bool Success = false;
	std::string ErrorMessage;

	operator bool() { return Success && ErrorMessage.empty(); }
};

ShaderCompileResult CompileShader(VertexShader_t handle, const ShaderCompileParams Params);
ShaderCompileResult CompileShader(PixelShader_t handle, const ShaderCompileParams Params);
ShaderCompileResult CompileShader(GeometryShader_t handle, const ShaderCompileParams Params);
ShaderCompileResult CompileShader(MeshShader_t handle, const ShaderCompileParams Params);
ShaderCompileResult CompileShader(AmplificationShader_t handle, const ShaderCompileParams Params);
ShaderCompileResult CompileShader(ComputeShader_t handle, const ShaderCompileParams Params);
ShaderCompileResult CompileShader(RaytracingRayGenShader_t handle, const ShaderCompileParams Params);
ShaderCompileResult CompileShader(RaytracingMissShader_t handle, const ShaderCompileParams Params);
ShaderCompileResult CompileShader(RaytracingAnyHitShader_t handle, const ShaderCompileParams Params);
ShaderCompileResult CompileShader(RaytracingClosestHitShader_t handle, const ShaderCompileParams Params);

}