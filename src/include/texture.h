#pragma once

enum class teTextureFormat { Invalid, BC1, BC1_SRGB, BC2, BC2_SRGB, BC3, BC3_SRGB, BC4U, BC4S, BC5U, BC5S, R32F, R32G32F, Depth32F, BGRA_sRGB, RGBA_sRGB, BGRA };
enum teTextureFlags { Empty = 0, GenerateMips = 1, RenderTexture = 2, UAV = 4 };
enum teTextureSampler { LinearClamp, LinearRepeat, NearestClamp, NearestRepeat, Anisotropic8Clamp, Anisotropic8Repeat };

struct teTexture2D
{
    unsigned index = 0;
    teTextureFormat format = teTextureFormat::Invalid;
    teTextureSampler sampler = teTextureSampler::LinearRepeat;
};

struct teTextureCube
{
    unsigned index = 0;
    teTextureFormat format = teTextureFormat::Invalid;
};

constexpr unsigned TextureCount = 80;

teTexture2D teCreateTexture2D( unsigned width, unsigned height, unsigned flags, teTextureFormat format, const char* debugName );
void teTextureGetDimension( teTexture2D texture, unsigned& outWidth, unsigned& outHeight );

// @param pixels Optional. If set, file is ignored and pixelsWidth, pixelsHeight and pixelsFormat must be provided.
teTexture2D teLoadTexture( const struct teFile& file, unsigned flags, void* pixels, int pixelsWidth, int pixelsHeight, teTextureFormat pixelsFormat );

teTextureCube teLoadTexture( const teFile& negX, const teFile& posX, const teFile& negY, const teFile& posY, const teFile& negZ, const teFile& posZ, unsigned flags );
