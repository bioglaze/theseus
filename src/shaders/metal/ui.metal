#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float2 uv;
    float4 color;
};

struct ImDrawVert
{
    float2  pos [[attribute(0)]];
    float2  uv [[attribute(1)]];
    uchar4  col [[attribute(2)]];
};

vertex ColorInOut uiVS( ImDrawVert in [[stage_in]], constant Uniforms& uniforms [[ buffer(1) ]] )
{
    ColorInOut out;

    out.position = uniforms.localToClip[ 0 ] * float4( in.pos, 0, 1 );
    out.uv = in.uv;
    out.color = float4( in.col.abgr ) / float4( 255.0f );

    return out;
}

fragment float4 uiPS( ColorInOut in [[stage_in]], texture2d<float, access::sample> textureMap [[texture(0)]] )
{
    constexpr sampler sampler0( coord::normalized, min_filter::linear, mag_filter::linear, mip_filter::linear );
    return in.color * textureMap.sample( sampler0, in.uv );
}
