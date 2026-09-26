#pragma once

#include "Textures.h"

namespace rl
{

bool AllocTextureImpl(Texture_t tex);
bool CreateTextureImpl(Texture_t tex, const TextureCreateDescEx& desc);
bool UpdateTextureImpl(Texture_t tex, const void* const data, uint32_t width, uint32_t height, RenderFormat format);
bool IsTextureUploadCompleteImpl(Texture_t tex);
void DestroyTexture(Texture_t tex);

}