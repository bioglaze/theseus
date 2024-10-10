#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float2 uv;
    float3 normal;
    float4 projCoord;
};

float linstep( float low, float high, float v )
{
    return saturate( (v - low) / (high - low) );
}

float VSM( float depth, float4 projCoord, texture2d<float, access::sample> shadowMap [[texture(1)]] )
{
    constexpr sampler sampler0( coord::normalized, address::repeat, filter::nearest );
    
    float2 uv = (projCoord.xy / projCoord.w) * 0.5f + 0.5f;
    float2 moments = shadowMap.sample( sampler0, uv ).rg;
    if (moments.x > depth)
        return 0.2f;
    return 1.0f;

/*    float variance = max( moments.y - moments.x * moments.x, -0.001f );

    float delta = depth - moments.x;
    float p = smoothstep( depth - 0.02f, depth, moments.x );
    float minAmbient = 0.2f;
    float pMax = linstep( minAmbient, 1.0f, variance / (variance + delta * delta) );

    return saturate( max( p, pMax ) );*/
}

vertex ColorInOut standardVS( uint vid [[ vertex_id ]],
                              constant Uniforms & uniforms [[ buffer(0) ]],
                              const device packed_float3* positions [[ buffer(1) ]],
                              const device packed_float2* uvs [[ buffer(2) ]],
                              const device packed_float3* normals [[ buffer(3) ]])
{
    ColorInOut out;

    out.position = uniforms.localToClip * float4( positions[ vid ], 1 );
    out.uv = float2( uvs[ vid ] );
    out.normal = (uniforms.localToView * float4( normals[ vid ], 1 )).xyz;
    out.projCoord = uniforms.localToShadowClip * float4( positions[ vid ], 1 );
    
    return out;
}

fragment float4 standardPS( ColorInOut in [[stage_in]], texture2d<float, access::sample> textureMap [[texture(0)]],
                            texture2d<float, access::sample> shadowMap [[texture(1)]])
{
    constexpr sampler sampler0( coord::normalized, address::repeat, filter::linear );
    
    float depth = (in.projCoord.z + 0.0001f) / in.projCoord.w;
    float shadow = max( 0.2f, VSM( depth, in.projCoord, shadowMap ) );

    return textureMap.sample( sampler0, in.uv ) * shadow;
}
