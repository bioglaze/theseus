#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float2 texCoords;
};

vertex ColorInOut fullscreenVS( uint vid [[ vertex_id ]] )
{
    ColorInOut out;

    out.texCoords = float2( (vid << 1) & 2, vid & 2 );
    out.position = float4( out.texCoords * 2.0f + -1.0f, 0.0f, 1.0f );
    return out;
}

fragment float4 fullscreenPS( ColorInOut in [[stage_in]],
                              texture2d<float, access::sample> textureMap [[texture(0)]],
                             constant Uniforms& uniforms [[ buffer(0) ]])
{
    constexpr sampler mysampler( coord::normalized, address::repeat, filter::nearest );

    return textureMap.sample( mysampler, float2( in.texCoords.x, /*1 -*/ in.texCoords.y ) );
}
