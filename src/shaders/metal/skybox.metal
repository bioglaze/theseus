#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float3 uv;
};

vertex ColorInOut skyboxVS( uint vid [[ vertex_id ]],
                          constant Uniforms & uniforms [[ buffer(0) ]],
                           const device packed_float3* positions [[ buffer(1) ]],
                           const device packed_float2* uvs [[ buffer(2) ]])
{
    ColorInOut out;

    out.position = uniforms.localToClip[ 0 ] * float4( positions[ vid ], 1 );
    out.uv = float3( positions[ vid ] );
    
    return out;
}

fragment float4 skyboxPS( ColorInOut in [[stage_in]], texturecube<float, access::sample> textureMap [[texture(0)]] )
{
    constexpr sampler sampler0( coord::normalized, address::repeat, filter::nearest );
    //return float4( 1, 0, 0, 1 );
    return textureMap.sample( sampler0, in.uv );
}
