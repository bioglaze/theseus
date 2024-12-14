#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float4 posVS;
    float2 uv;
};

vertex ColorInOut momentsVS( uint vid [[ vertex_id ]],
                             constant Uniforms & uniforms [[ buffer(0) ]],
                             const device packed_float3* positions [[ buffer(1) ]],
                             const device packed_float2* uvs [[ buffer(2) ]])
{
    ColorInOut out;

    out.position = uniforms.localToClip * float4( positions[ vid ], 1 );
    out.posVS = uniforms.localToView * float4( positions[ vid ], 1 );
    out.uv = float2( uvs[ vid ] );
    
    return out;
}

fragment float4 momentsPS( ColorInOut in [[stage_in]], texture2d<float, access::sample> textureMap [[texture(0)]])
{
    float distToLight = length( in.posVS );

    float dx = dfdx( distToLight );
    float dy = dfdy( distToLight );

    float moment1 = distToLight;
    float moment2 = distToLight * distToLight + 0.25f * (dx * dx + dy * dy);

    return float4( moment1, moment2, 0.0f, 1.0f );
}
