#include <stdio.h>
#include <stdint.h>
#include "file.h"
#include "texture.h"
#include "te_stdlib.h"

#define DDS_MAGIC 0x20534444

//  DDS_header.dwFlags
#define DDSD_CAPS                   0x00000001 
#define DDSD_HEIGHT                 0x00000002
#define DDSD_WIDTH                  0x00000004
#define DDSD_PITCH                  0x00000008
#define DDSD_PIXELFORMAT            0x00001000
#define DDSD_MIPMAPCOUNT            0x00020000
#define DDSD_LINEARSIZE             0x00080000 
#define DDSD_DEPTH                  0x00800000 

//  DDS_header.sPixelFormat.dwFlags
#define DDPF_ALPHAPIXELS            0x00000001 
#define DDPF_FOURCC                 0x00000004
#define DDPF_INDEXED                0x00000020 
#define DDPF_RGB                    0x00000040 

//  DDS_header.sCaps.dwCaps1
#define DDSCAPS_COMPLEX             0x00000008 
#define DDSCAPS_TEXTURE             0x00001000 
#define DDSCAPS_MIPMAP              0x00400000 

//  DDS_header.sCaps.dwCaps2
#define DDSCAPS2_CUBEMAP            0x00000200 
#define DDSCAPS2_CUBEMAP_POSITIVEX  0x00000400 
#define DDSCAPS2_CUBEMAP_NEGATIVEX  0x00000800 
#define DDSCAPS2_CUBEMAP_POSITIVEY  0x00001000 
#define DDSCAPS2_CUBEMAP_NEGATIVEY  0x00002000 
#define DDSCAPS2_CUBEMAP_POSITIVEZ  0x00004000 
#define DDSCAPS2_CUBEMAP_NEGATIVEZ  0x00008000 
#define DDSCAPS2_VOLUME             0x00200000 

#define D3DFMT_DXT1 827611204
#define D3DFMT_DXT3 861165636
#define D3DFMT_DXT5 894720068

#define MAKEFOURCC(ch0, ch1, ch2, ch3)                              \
((uint32_t)(uint8_t)(ch0) | ((uint32_t)(uint8_t)(ch1) << 8) |       \
((uint32_t)(uint8_t)(ch2) << 16) | ((uint32_t)(uint8_t)(ch3) << 24))

#define PF_IS_DXT1(pf) ((pf.dwFlags & DDPF_FOURCC) && (pf.dwFourCC == D3DFMT_DXT1))
#define PF_IS_DXT3(pf) ((pf.dwFlags & DDPF_FOURCC) && (pf.dwFourCC == D3DFMT_DXT3))
#define PF_IS_DXT5(pf) ((pf.dwFlags & DDPF_FOURCC) && (pf.dwFourCC == D3DFMT_DXT5))
#define PF_IS_BC4U(pf) ((pf.dwFlags & DDPF_FOURCC) && (pf.dwFourCC == MAKEFOURCC('B', 'C', '4', 'U') ))
#define PF_IS_BC4S(pf) ((pf.dwFlags & DDPF_FOURCC) && (pf.dwFourCC == MAKEFOURCC('B', 'C', '4', 'S') ))
#define PF_IS_BC5U(pf) ((pf.dwFlags & DDPF_FOURCC) && (pf.dwFourCC == MAKEFOURCC('B', 'C', '5', 'U') ))
#define PF_IS_BC5S(pf) ((pf.dwFlags & DDPF_FOURCC) && (pf.dwFourCC == MAKEFOURCC('B', 'C', '5', 'S') ))
#define PF_IS_ATI1(pf) ((pf.dwFlags & DDPF_FOURCC) && (pf.dwFourCC == MAKEFOURCC('A', 'T', 'I', '1') ))
#define PF_IS_ATI2(pf) ((pf.dwFlags & DDPF_FOURCC) && (pf.dwFourCC == MAKEFOURCC('A', 'T', 'I', '2') ))
#define PF_IS_DX10(pf) ((pf.dwFlags & DDPF_FOURCC) && (pf.dwFourCC == MAKEFOURCC('D', 'X', '1', '0') ))

constexpr unsigned dxgiFormat_BC1_UNORM = 71;
constexpr unsigned dxgiFormat_BC1_UNORM_SRGB = 72;
constexpr unsigned dxgiFormat_BC2_UNORM = 74;
constexpr unsigned dxgiFormat_BC2_UNORM_SRGB = 75;
constexpr unsigned dxgiFormat_BC3_UNORM = 77;
constexpr unsigned dxgiFormat_BC3_UNORM_SRGB = 78;
constexpr unsigned dxgiFormat_BC4_UNORM = 80;
constexpr unsigned dxgiFormat_BC4_SNORM = 81;
constexpr unsigned dxgiFormat_BC5_UNORM = 83;
constexpr unsigned dxgiFormat_BC5_SNORM = 84;

teTextureFormat DXGIFormatToTeTextureFormat( unsigned dxgiFormat )
{
    if (dxgiFormat == dxgiFormat_BC1_UNORM) return teTextureFormat::BC1;
    if (dxgiFormat == dxgiFormat_BC1_UNORM_SRGB) return teTextureFormat::BC1_SRGB;
    if (dxgiFormat == dxgiFormat_BC2_UNORM) return teTextureFormat::BC2;
    if (dxgiFormat == dxgiFormat_BC2_UNORM_SRGB) return teTextureFormat::BC2_SRGB;
    if (dxgiFormat == dxgiFormat_BC3_UNORM) return teTextureFormat::BC3;
    if (dxgiFormat == dxgiFormat_BC3_UNORM_SRGB) return teTextureFormat::BC3_SRGB;
    if (dxgiFormat == dxgiFormat_BC4_UNORM) return teTextureFormat::BC4U;
    if (dxgiFormat == dxgiFormat_BC4_SNORM) return teTextureFormat::BC4S;
    if (dxgiFormat == dxgiFormat_BC5_UNORM) return teTextureFormat::BC5U;
    if (dxgiFormat == dxgiFormat_BC5_SNORM) return teTextureFormat::BC5S;

    teAssert( !"Unhandled format!" );
    return teTextureFormat::Invalid;
}

static inline unsigned MyMax2( unsigned x, unsigned y ) noexcept
{
    return x > y ? x : y;
}

struct DDSInfo
{
    unsigned divSize;
    unsigned blockBytes;
};

union DDSHeader
{
    struct
    {
        uint32_t dwMagic;
        uint32_t dwSize;
        uint32_t dwFlags;
        uint32_t dwHeight;
        uint32_t dwWidth;
        uint32_t dwPitchOrLinearSize;
        uint32_t dwDepth;
        uint32_t dwMipMapCount;
        uint32_t dwReserved1[ 11 ];

        // DDPIXELFORMAT
        struct
        {
            uint32_t dwSize;
            uint32_t dwFlags;
            uint32_t dwFourCC;
            uint32_t dwRGBBitCount;
            uint32_t dwRBitMask;
            uint32_t dwGBitMask;
            uint32_t dwBBitMask;
            uint32_t dwAlphaBitMask;
        } sPixelFormat;

        // DDCAPS2
        struct
        {
            uint32_t dwCaps1;
            uint32_t dwCaps2;
            uint32_t dwDDSX;
            uint32_t dwReserved;
        } sCaps;

        uint32_t dwReserved2;
    } sHeader;

    uint8_t data[ 128 ];
};

struct DDSHeaderDX10
{
    uint32_t DXGIFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t reserved;
};

bool LoadDDS( const teFile& fileContents, unsigned& outWidth, unsigned& outHeight, teTextureFormat& outFormat, unsigned& outMipLevelCount, unsigned( &outMipOffsets )[ 15 ] )
{
    DDSHeader header;
    DDSHeaderDX10 header10;

    if (!fileContents.data)
    {
        outWidth = 512;
        outHeight = 512;
        return false;
    }

    if (fileContents.size < (unsigned)sizeof( header ))
    {
        printf( "DDS loader error: Texture %s file length is less than DDS header length.\n", fileContents.path );
        return false;
    }

    teMemcpy( &header, fileContents.data, sizeof( header ) );

    teAssert( header.sHeader.dwMagic == DDS_MAGIC && "DDSLoader: Wrong magic" );
    teAssert( header.sHeader.dwSize == 124 && "DDSLoader: Wrong header size" );

    if (!(header.sHeader.dwFlags & DDSD_PIXELFORMAT) || !(header.sHeader.dwFlags & DDSD_CAPS))
    {
        printf( "DDS loader error: Texture %s doesn't contain pixelformat or caps.\n", fileContents.path );
        outWidth = 32;
        outHeight = 32;
        return false;
    }

    outWidth = header.sHeader.dwWidth;
    outHeight = header.sHeader.dwHeight;
    teAssert( !(outWidth & (outWidth - 1)) && "DDSLoader: Wrong image width" );
    teAssert( !(outHeight & (outHeight - 1)) && "DDSLoader: Wrong image height" );

    constexpr DDSInfo loadInfoDXT1 = { 4, 8 };
    constexpr DDSInfo loadInfoDXT3 = { 4, 16 };
    constexpr DDSInfo loadInfoDXT5 = { 4, 16 };

    DDSInfo li = {};
    size_t additionalFileOffset = 0;

    if (PF_IS_DXT1( header.sHeader.sPixelFormat ))
    {
        li = loadInfoDXT1;
        outFormat = teTextureFormat::BC1_SRGB;
    }
    else if (PF_IS_DXT3( header.sHeader.sPixelFormat ))
    {
        li = loadInfoDXT3;
        outFormat = teTextureFormat::BC2_SRGB;
    }
    else if (PF_IS_DXT5( header.sHeader.sPixelFormat ))
    {
        li = loadInfoDXT5;
        outFormat = teTextureFormat::BC3_SRGB;
    }
    else if (PF_IS_BC4S( header.sHeader.sPixelFormat ))
    {
        li = loadInfoDXT5;
        outFormat = teTextureFormat::BC4S;
    }
    else if (PF_IS_BC4U( header.sHeader.sPixelFormat ))
    {
        li = loadInfoDXT5;
        outFormat = teTextureFormat::BC4U;
    }
    else if (PF_IS_BC5S( header.sHeader.sPixelFormat ))
    {
        li = loadInfoDXT5;
        outFormat = teTextureFormat::BC5S;
    }
    else if (PF_IS_BC5U( header.sHeader.sPixelFormat ))
    {
        li = loadInfoDXT5;
        outFormat = teTextureFormat::BC5U;
    }
    else if (PF_IS_DX10( header.sHeader.sPixelFormat ))
    {
        teMemcpy( &header10, fileContents.data + sizeof( header ), sizeof( header10 ) );
        li = (header10.DXGIFormat == dxgiFormat_BC1_UNORM || header10.DXGIFormat == dxgiFormat_BC1_UNORM_SRGB) ? loadInfoDXT1 : loadInfoDXT5;
        outFormat = DXGIFormatToTeTextureFormat( header10.DXGIFormat );
        additionalFileOffset = sizeof( header10 );
    }
    else if (PF_IS_ATI1( header.sHeader.sPixelFormat ))
    {
        li = loadInfoDXT1;
        outFormat = teTextureFormat::BC4U;
    }
    else if (PF_IS_ATI2( header.sHeader.sPixelFormat ))
    {
        li = loadInfoDXT1;
        outFormat = teTextureFormat::BC5U;
    }
    else
    {
        printf( "DDS loader error: Texture %s has unknown pixelformat.\n", fileContents.path );
        outWidth = 32;
        outHeight = 32;
        outFormat = teTextureFormat::Invalid;
        return false;
    }

    outMipLevelCount = (header.sHeader.dwFlags & DDSD_MIPMAPCOUNT) ? header.sHeader.dwMipMapCount : 1;
    teAssert( outMipLevelCount < 16 && "Too many mipmap levels!" );

    size_t fileOffset = sizeof( header ) + additionalFileOffset;
    unsigned x = outWidth;
    unsigned y = outHeight;

    size_t size = MyMax2( li.divSize, x ) / li.divSize * (size_t)MyMax2( li.divSize, y ) / li.divSize * (size_t)li.blockBytes;
    teAssert( (header.sHeader.dwFlags & DDSD_LINEARSIZE) != 0 && "DDSLoader, need flag DDSD_LINEARSIZE" );

    if (size == 0)
    {
        printf( "DDS loader error: Texture %s contents are empty.\n", fileContents.path );
        outWidth = 32;
        outHeight = 32;
        return false;
    }

    for (unsigned ix = 0; ix < outMipLevelCount; ++ix)
    {
        outMipOffsets[ ix ] = (int)fileOffset;

        fileOffset += size;
        x = (x + 1) >> 1;
        y = (y + 1) >> 1;
        size = MyMax2( li.divSize, x ) / (size_t)li.divSize * MyMax2( li.divSize, y ) / li.divSize * (size_t)li.blockBytes;
    }

    return true;
}

void LoadTGA( const teFile& file, unsigned& outWidth, unsigned& outHeight, unsigned& outDataBeginOffset, unsigned& outBitsPerPixel, unsigned char** outPixelData )
{
    unsigned char* data = (unsigned char*)file.data;

    unsigned offs = 0;
    offs++;
    offs++;
    int imageType = data[ offs++ ];

    if (imageType != 2)
    {
        printf( "Incompatible .tga file: %s. Must be truecolor and not use RLE.\n", file.path );
        return;
    }

    offs += 5; // colorSpec
    offs += 4; // specBegin

    unsigned width = data[ offs ] | (data[ offs + 1 ] << 8);
    ++offs;

    unsigned height = data[ offs + 1 ] | (data[ offs + 2 ] << 8);

    ++offs;

    outWidth = width;
    outHeight = height;

    offs += 4; // specEnd
    outBitsPerPixel = data[ offs - 2 ];

    *outPixelData = (unsigned char*)teMalloc( width * height * 4 );
    teMemcpy( *outPixelData, data, width * outBitsPerPixel / 8 );

    const int topLeft = data[ offs - 1 ] & 32;

    if (topLeft != 32)
    {
        printf( "%s image origin is not top left.\n", file.path );
    }

    outDataBeginOffset = offs;
}
