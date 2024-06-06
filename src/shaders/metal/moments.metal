#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float2 uv;
};

vertex ColorInOut momentsVS( uint vid [[ vertex_id ]],
                          constant Uniforms & uniforms [[ buffer(0) ]],
                           const device packed_float3* positions [[ buffer(1) ]],
                           const device packed_float2* uvs [[ buffer(2) ]])
{
    ColorInOut out;

    out.position = uniforms.localToClip * float4( positions[ vid ], 1 );
    out.uv = float2( uvs[ vid ] );
    
    return out;
}

fragment float4 momentsPS( ColorInOut in [[stage_in]], texture2d<float, access::sample> textureMap [[texture(0)]])
{
    float linearDepth = in.position.z;

    float dx = dfdx( linearDepth );
    float dy = dfdy( linearDepth );

    float moment1 = linearDepth;
    float moment2 = linearDepth * linearDepth + 0.25f * (dx * dx + dy * dy);

    return float4( moment1, moment2, 0.0f, 1.0f );
}
