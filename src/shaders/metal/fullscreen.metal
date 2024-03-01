#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float2 texCoords;
};

vertex ColorInOut fullscreenVS( uint vid [[ vertex_id ]],
                                constant Uniforms & uniforms [[ buffer(0) ]],
                                const device packed_float3* positions [[ buffer(1) ]],
                                const device packed_float2* uvs [[ buffer(2) ]])
{
    ColorInOut vsOut;

    //out.texCoords = float2( (vid << 1) & 2, vid & 2 );
    //out.position = float4( out.texCoords * 2.0f + -1.0f, 0.0f, 1.0f );
    vsOut.position = float4( positions[ vid ], 1 );
    vsOut.position.xy *= uniforms.tilesXY.xy;
    vsOut.position.xy += uniforms.tilesXY.wz;
    vsOut.texCoords = uvs[ vid ];

    return vsOut;
}

fragment float4 fullscreenPS( ColorInOut in [[stage_in]],
                              texture2d<float, access::sample> textureMap [[texture(0)]],
                             constant Uniforms& uniforms [[ buffer(0) ]])
{
    constexpr sampler mysampler( coord::normalized, address::repeat, filter::nearest );

    return textureMap.sample( mysampler, in.texCoords );
}

fragment float4 fullscreenAdditivePS( ColorInOut in [[stage_in]],
                                      texture2d<float, access::sample> textureMap [[texture(0)]],
                                      constant Uniforms& uniforms [[ buffer(0) ]])
{
    constexpr sampler mysampler( coord::normalized, address::repeat, filter::nearest );

    return textureMap.sample( mysampler, in.texCoords.xy ).rrrr;
}

