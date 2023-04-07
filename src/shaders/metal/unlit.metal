#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float2 uv;
};

vertex ColorInOut unlitVS( uint vid [[ vertex_id ]],
                          constant Uniforms & uniforms [[ buffer(0) ]],
                           const device packed_float3* positions [[ buffer(1) ]],
                           const device packed_float2* uvs [[ buffer(2) ]])
{
    ColorInOut out;

    out.position = uniforms.localToClip[ 0 ] * float4( positions[ vid /*+ uniforms.posOffset*/ ], 1 );
    out.uv = float2( uvs[ vid /*+ uniforms.posOffset*/ ] );
    
    return out;
}

fragment float4 unlitPS( ColorInOut in [[stage_in]], texture2d<float, access::sample> textureMap [[texture(0)]]/*, sampler sampler0 [[sampler(0)]]*/ )
{
    constexpr sampler sampler0( coord::normalized, address::repeat, filter::nearest );
    //return float4( 1, 0, 0, 1 );
    return textureMap.sample( sampler0, in.uv );
}
