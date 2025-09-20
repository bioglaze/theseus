#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

#define TILE_RES 16
#define LIGHT_INDEX_BUFFER_SENTINEL 0x7fffffff

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

uint GetTileIndex( float2 screenPos, float2 screenDim )
{
    const float tileRes = (float) TILE_RES;
    uint numCellsX = (screenDim.x + TILE_RES - 1) / TILE_RES; // screenDim.x is screen width in pixels
    uint tileIdx = floor( screenPos.x / tileRes ) + floor( screenPos.y / tileRes ) * numCellsX;
    return tileIdx;
}

float linstep( float low, float high, float v )
{
    return saturate( (v - low) / (high - low) );
}

float VSM( float depth, float4 projCoord, texture2d<float, access::sample> shadowMap [[texture(2)]] )
{
    constexpr sampler sampler0( coord::normalized, address::clamp_to_zero, filter::linear );
    
    // NOTE: This is needed on Metal, but not on Vulkan.
    projCoord.y = -projCoord.y;

    float2 uv = (projCoord.xy / projCoord.w) * 0.5f + 0.5f;

    float2 moments = shadowMap.sample( sampler0, uv ).rg;

    float variance = max( moments.y - moments.x * moments.x, -0.001f );

    float delta = depth - moments.x;
    float p = smoothstep( depth - 0.02f, depth, moments.x );
    float minAmbient = 0.2f;
    float pMax = linstep( minAmbient, 1.0f, variance / (variance + delta * delta) );

    return saturate( max( p, pMax ) );
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
    out.normalVS = (uniforms.localToView * float4( normals[ vid ], 0 )).xyz;
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

float pointLightAttenuation( float d, float r )
{
    return 2.0f / (d * d + r * r + d * sqrt( d * d + r * r ));
}

float D_GGX( float dotNH, float a )
{
    float a2 = a * a;
    float f = (dotNH * a2 - dotNH) * dotNH + 1.0f;
    return a2 / (3.14159265f * f * f);
}

float3 F_Schlick( float dotVH, float3 f0 )
{
    return f0 + (float3( 1, 1, 1 ) - f0) * pow( 1.0f - dotVH, 5.0f );
}

float V_SmithGGXCorrelated( float dotNV, float dotNL, float a )
{
    float a2 = a * a;
    float GGXL = dotNV * sqrt( (-dotNL * a2 + dotNL) * dotNL + a2 );
    float GGXV = dotNL * sqrt( (-dotNV * a2 + dotNV) * dotNV + a2 );
    return 0.5f / (GGXV + GGXL);
}

float Fd_Lambert()
{
    return 1.0f / 3.14159265f;
}

fragment float4 standardPS( ColorInOut in [[stage_in]], texture2d<float, access::sample> textureMap [[texture(0)]],
                            constant Uniforms& uniforms [[ buffer(0) ]],
                            texture2d<float, access::sample> normalMap [[texture(1)]],
                            texture2d<float, access::sample> shadowMap [[texture(2)]],
                            const device uint* lightIndexBuf [[ buffer(1) ]],
                            const device float4* pointLightBufferCenterAndRadius [[ buffer(2) ]],
                            const device float4* pointLightBufferColors [[ buffer(3) ]])
{
    constexpr sampler sampler0( coord::normalized, address::repeat, filter::linear );
    
    float surfaceDistToLight = length( uniforms.lightPosition.xyz - in.positionWS );
    float shadow = max( 0.2f, VSM( surfaceDistToLight, in.projCoord, shadowMap ) );
    //shadow = 1;

    float2 normalTex = normalMap.sample( sampler0, in.uv ).xy;
    float3 normalTS = float3( normalTex.x, normalTex.y, sqrt( 1 - normalTex.x * normalTex.x - normalTex.y * normalTex.y ) );
    float3 normalVS = tangentSpaceTransform( in.tangentVS, in.bitangentVS, in.normalVS, normalTS.xyz );
    
    const float3 N = normalize( normalVS );
    const float3 V = normalize( in.positionVS );
    //const float3 L = -uniforms.lightDir.xyz;
    //const float3 H = normalize( L + V );
    float specularStrength = 0.5;
    
    float3 ambient = float3( 0.2, 0.2, 0.2 );
    float3 accumDiffuseAndSpecular = uniforms.lightColor.rgb;
    const float3 surfaceToLightVS = ( uniforms.localToView * uniforms.lightDir ).xyz;
    float dotNL = saturate( dot( normalVS, -surfaceToLightVS ) );
    const float dotNV = abs( dot( N, V ) ) + 1e-5f;
    
    accumDiffuseAndSpecular *= dotNL;
    
    float3 reflectDir = reflect( -surfaceToLightVS, normalVS );
    float spec = pow( max( dot( V, reflectDir ), 0.0 ), 32 );
    float3 specular = specularStrength * spec;
    accumDiffuseAndSpecular += specular;

    float4 albedo = textureMap.sample( sampler0, in.uv );

    const uint tileIndex = GetTileIndex( in.position.xy, uniforms.tilesXY.xy );
    uint index = uniforms.maxLightsPerTile * tileIndex;
    uint nextLightIndex = lightIndexBuf[ index ];

    // Point lights
    while (nextLightIndex != LIGHT_INDEX_BUFFER_SENTINEL)
    {
        const int lightIndex = nextLightIndex;
        index++;
        nextLightIndex = lightIndexBuf[ index ];

        const float4 center = pointLightBufferCenterAndRadius[ lightIndex ];
        const float radius = center.w;
        const float3 vecToLightWS = center.xyz - in.positionWS.xyz;
        const float lightDistance = length( vecToLightWS );
        
        if (lightDistance < radius)
        {
            const float3 vecToLightVS = (uniforms.localToView * float4( vecToLightWS, 0 )).xyz;
            const float3 L = normalize( -vecToLightVS );
            const float3 H = normalize( L + V );
            
            const float dotNL = saturate( dot( N, -L ) );
            const float dotLH = saturate( dot( L, H ) );
            const float dotNH = saturate( dot( N, H ) );
            
            const float3 f0 = float3( 0.04f );
            
            float roughness = 0.5f;
            const float a = roughness * roughness;
            const float D = D_GGX( dotNH, a );
            const float3 F = F_Schlick( dotLH, f0 );
            const float v = V_SmithGGXCorrelated( dotNV, dotNL, a );
            const float3 Fr = (D * v) * F;
            const float3 Fd = Fd_Lambert();

            const float attenuation = pointLightAttenuation( length( vecToLightWS ), 1.0f / radius );
            const float3 color = Fd + Fr;
            accumDiffuseAndSpecular.rgb += (color * pointLightBufferColors[ lightIndex ].rgb) * attenuation * dotNL;
            return pointLightBufferColors[ lightIndex ];
        }
    }

    accumDiffuseAndSpecular = max( ambient, accumDiffuseAndSpecular );
    return albedo * float4( saturate( accumDiffuseAndSpecular ) * shadow, 1 );
}
