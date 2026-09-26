add_library(RenderDx11
            "Render/Binding.h"
            "Render/Buffers.h"
            "Render/CommandList.h"
            "Render/IndirectCommands.h"
            "Render/PipelineState.h"
            "Render/Raytracing.h"
            "Render/Render.h"
            "Render/RenderDefines.h"
		    "Render/RenderPtr.h"
            "Render/RenderTypes.h"
            "Render/RootSignature.h"
            "Render/Samplers.h"
            "Render/Shaders.h"
            "Render/Textures.h"
            "Render/TextureInfo.h"
            "Render/View.h"
)

add_library(RenderDx12
            "Render/Binding.h"
            "Render/Buffers.h"
            "Render/CommandList.h"
            "Render/IndirectCommands.h"
            "Render/PipelineState.h"
            "Render/Raytracing.h"
            "Render/Render.h"
            "Render/RenderDefines.h"
		    "Render/RenderPtr.h"
            "Render/RenderTypes.h"
            "Render/RootSignature.h"
            "Render/Samplers.h"
            "Render/Shaders.h"
            "Render/Textures.h"
            "Render/TextureInfo.h"
            "Render/View.h"
)

set_target_properties(RenderDx11 PROPERTIES OUTPUT_NAME_DEBUG RenderExampleDx11_Debug)
set_target_properties(RenderDx12 PROPERTIES OUTPUT_NAME_DEBUG RenderExampleDx12_Debug)

target_include_directories(RenderDx11 PRIVATE 
                            "Render" 
                            "Private"
)

target_include_directories(RenderDx12 PRIVATE 
                            "Render" 
                            "Private"
)

target_sources(RenderDx11 PRIVATE
                "Private/Binding.cpp"
                "Private/Buffers.cpp"
                "Private/IDArray.h"
                "Private/IndirectCommands.cpp"
                "Private/PipelineState.cpp"
                "Private/Raytracing.cpp"
                "Private/RootSignature.cpp"
                "Private/Shaders.cpp"
                "Private/SparseArray.h"
                "Private/Textures.cpp"
                "Private/Impl/BindingImpl.h"
                "Private/Impl/BuffersImpl.h"
                "Private/Impl/IndirectCommandsImpl.h"
                "Private/Impl/PipelineStateImpl.h"
                "Private/Impl/RaytracingImpl.h"
                "Private/Impl/RootSignatureImpl.h"
                "Private/Impl/ShadersImpl.h"
                "Private/Impl/TexturesImpl.h"
)

target_sources(RenderDx12 PRIVATE
                "Private/Binding.cpp"
                "Private/Buffers.cpp"
                "Private/IDArray.h"
                "Private/IndirectCommands.cpp"
                "Private/PipelineState.cpp"
                "Private/Raytracing.cpp"
                "Private/RootSignature.cpp"
                "Private/Shaders.cpp"
                "Private/SparseArray.h"
                "Private/Textures.cpp"
                "Private/Impl/BindingImpl.h"
                "Private/Impl/BuffersImpl.h"
                "Private/Impl/IndirectCommandsImpl.h"
                "Private/Impl/PipelineStateImpl.h"
                "Private/Impl/RaytracingImpl.h"
                "Private/Impl/RootSignatureImpl.h"
                "Private/Impl/ShadersImpl.h"
                "Private/Impl/TexturesImpl.h"
)

target_sources(RenderDx11 PRIVATE
                "Private/Impl/Dx/d3dx12.h"
                "Private/Impl/Dx/DxErrorHandling.cpp"
                "Private/Impl/Dx/DxErrorHandling.h"
                "Private/Impl/Dx11/BindingImpl.cpp"
                "Private/Impl/Dx11/BuffersImpl.cpp"
                "Private/Impl/Dx11/CommandListImpl.cpp"
                "Private/Impl/Dx11/Dx11Types.cpp"
                "Private/Impl/Dx11/Dx11Types.h"
                "Private/Impl/Dx11/IndirectCommandsImpl.cpp"
                "Private/Impl/Dx11/PipelineStateImpl.cpp"
                "Private/Impl/Dx12/RaytracingImpl.cpp"
                "Private/Impl/Dx11/RenderImpl.cpp"
                "Private/Impl/Dx11/RenderImpl.h"
                "Private/Impl/Dx11/RootSignatureImpl.cpp"
                "Private/Impl/Dx11/ShadersImpl.cpp"
                "Private/Impl/Dx11/TexturesImpl.cpp"
                "Private/Impl/Dx11/ViewImpl.cpp"
)

target_sources(RenderDx12 PRIVATE
                "Private/Impl/Dx/d3dx12.h"
                "Private/Impl/Dx/DxErrorHandling.cpp"
                "Private/Impl/Dx/DxErrorHandling.h"
                "Private/Impl/Dx12/BindingImpl.cpp"
                "Private/Impl/Dx12/CommandListImpl.cpp"
                "Private/Impl/Dx12/Dx12Types.cpp"
                "Private/Impl/Dx12/Dx12Types.h"
                "Private/Impl/Dx12/DynamicBuffersImpl.cpp"
                "Private/Impl/Dx12/FrameConstantsImpl.cpp"
                "Private/Impl/Dx12/IndirectCommandsImpl.cpp"
                "Private/Impl/Dx12/PipelineStateImpl.cpp"
                "Private/Impl/Dx12/RaytracingImpl.cpp"
                "Private/Impl/Dx12/RenderImpl.cpp"
                "Private/Impl/Dx12/RenderImpl.h"
                "Private/Impl/Dx12/RootSignatureImpl.cpp"
                "Private/Impl/Dx12/SamplersImpl.cpp"
                "Private/Impl/Dx12/StaticBuffersImpl.cpp"
                "Private/Impl/Dx12/ShadersImpl.cpp"
                "Private/Impl/Dx12/TexturesImpl.cpp"
                "Private/Impl/Dx12/ViewImpl.cpp"
                "Private/Impl/Dxc/DXCompiler.cpp"
                "Private/Impl/Dxc/DXCompiler.h"
)

# Dx11 only needs to copy the dxcompiler

add_custom_command(TARGET RenderDx11 POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
"${CMAKE_CURRENT_SOURCE_DIR}/redist/dxcompiler.dll"
"${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/dxcompiler.dll")

add_custom_command(TARGET RenderDx11 POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
"${CMAKE_CURRENT_SOURCE_DIR}/redist/dxcompiler.dll"
"${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}/dxcompiler.dll")

# Dx12 must copy both dxcompiler and dxil

add_custom_command(TARGET RenderDx12 POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
"${CMAKE_CURRENT_SOURCE_DIR}/redist/dxcompiler.dll"
"${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/dxcompiler.dll")

add_custom_command(TARGET RenderDx12 POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
"${CMAKE_CURRENT_SOURCE_DIR}/redist/dxcompiler.dll"
"${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}/dxcompiler.dll")

add_custom_command(TARGET RenderDx12 POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
"${CMAKE_CURRENT_SOURCE_DIR}/redist/dxil.dll"
"${CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE}/dxil.dll")

add_custom_command(TARGET RenderDx12 POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy_if_different
"${CMAKE_CURRENT_SOURCE_DIR}/redist/dxil.dll"
"${CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG}/dxil.dll")