#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float2 uv;
    float3 normalVS;
    float4 projCoord;
    float3 positionVS;
    float3 positionWS;
    float3 tangentVS;
    float3 bitangentVS;
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
                              const device packed_float3* normals [[ buffer(3) ]],
                              const device packed_float4* tangents [[ buffer(4) ]])
{
    ColorInOut out;

    out.position = uniforms.localToClip * float4( positions[ vid ], 1 );
    out.uv = float2( uvs[ vid ] );
    out.normalVS = (uniforms.localToView * float4( normals[ vid ], 1 )).xyz;
    out.projCoord = uniforms.localToShadowClip * float4( positions[ vid ], 1 );
    out.positionVS = (uniforms.localToView * float4( positions[ vid ], 1 )).xyz;
    out.positionWS = (uniforms.localToWorld * float4( positions[ vid ], 1 )).xyz;
    out.tangentVS = (uniforms.localToView * float4( tangents[ vid ].xyz, 0 )).xyz;
    float3 ct = cross( normals[ vid ], tangents[ vid ].xyz ) * tangents[ vid ].w;
    out.bitangentVS = (uniforms.localToView * float4( ct, 0 ) ).xyz;

    return out;
}

float3 tangentSpaceTransform( float3 tangent, float3 bitangent, float3 normal, float3 v )
{
    return normalize( v.x * tangent + v.y * bitangent + v.z * normal );
}

fragment float4 standardPS( ColorInOut in [[stage_in]], texture2d<float, access::sample> textureMap [[texture(0)]],
                            constant Uniforms & uniforms [[ buffer(0) ]],
                            texture2d<float, access::sample> normalMap [[texture(1)]],
                            texture2d<float, access::sample> shadowMap [[texture(2)]])
{
    constexpr sampler sampler0( coord::normalized, address::repeat, filter::linear );
    
    float surfaceDistToLight = length( uniforms.lightPosition.xyz - in.positionWS );
    float shadow = max( 0.2f, VSM( surfaceDistToLight, in.projCoord, shadowMap ) );

    float2 normalTex = normalMap.sample( sampler0, in.uv ).xy;
    float3 normalTS = float3( normalTex.x, normalTex.y, sqrt( 1 - normalTex.x * normalTex.x - normalTex.y * normalTex.y ) );
    float3 normalVS = tangentSpaceTransform( in.tangentVS, in.bitangentVS, in.normalVS, normalTS.xyz );

    const float3 V = normalize( in.positionVS );
    const float3 L = -uniforms.lightDir.xyz;
    const float3 H = normalize( L + V );
    float specularStrength = 0.5;
    
    float3 ambient = float3( 0.2, 0.2, 0.2 );
    float3 accumDiffuseAndSpecular = uniforms.lightColor.rgb;
    const float3 surfaceToLightVS = ( uniforms.localToView * uniforms.lightDir ).xyz;
    float dotNL = saturate( dot( normalVS, -surfaceToLightVS ) );
    accumDiffuseAndSpecular *= dotNL;
    
    float3 reflectDir = reflect( -surfaceToLightVS, normalVS );
    float spec = pow( max( dot( V, reflectDir ), 0.0 ), 32 );
    float3 specular = specularStrength * spec;
    accumDiffuseAndSpecular += specular;


    float4 albedo = textureMap.sample( sampler0, in.uv );

    return albedo * float4( saturate( accumDiffuseAndSpecular + ambient ) * shadow, 1 );
}
