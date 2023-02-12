#pragma once

struct teTexture2D
{
    unsigned index = 0;
};

enum class teTextureFormat { Invalid, BC1, BC2, BC3, BC4U, BC4S, BC5U, BC5S, R32F, R32G32F, Depth32F, BGRA_sRGB };
enum teTextureFlags { Empty = 0, GenerateMips = 1, SRGB = 2, RenderTexture = 4, UAV = 8 };

teTexture2D teCreateTexture2D( unsigned width, unsigned height, unsigned flags, teTextureFormat format, const char* debugName );