#pragma once

enum class teTextureFormat { Invalid, BC1, BC2, BC3, BC4U, BC4S, BC5U, BC5S, R32F, R32G32F, Depth32F, BGRA_sRGB, RGBA_sRGB, BGRA };
enum teTextureFlags { Empty = 0, GenerateMips = 1, SRGB = 2, RenderTexture = 4, UAV = 8 };
enum teTextureFilter { LinearRepeat };

struct teTexture2D
{
    unsigned index = 0;
    teTextureFormat format = teTextureFormat::Invalid;
};

struct teTextureCube
{
    unsigned index = 0;
    teTextureFormat format = teTextureFormat::Invalid;
};

constexpr unsigned TextureCount = 80;

teTexture2D teCreateTexture2D( unsigned width, unsigned height, unsigned flags, teTextureFormat format, const char* debugName );
void teTextureGetDimension( teTexture2D texture, unsigned& outWidth, unsigned& outHeight );
teTexture2D teLoadTexture( const struct teFile& file, unsigned flags );
teTextureCube teLoadTexture( const teFile& negX, const teFile& posX, const teFile& negY, const teFile& posY, const teFile& negZ, const teFile& posZ, unsigned flags, teTextureFilter filter );