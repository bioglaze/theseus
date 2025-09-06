#include "ubo.h"

#define TILE_RES 16
#define LIGHT_INDEX_BUFFER_SENTINEL 0x7fffffff

struct VSOutput
{
    float4 pos         : SV_Position;
    float2 uv          : TEXCOORD0;
    float4 projCoord   : TEXCOORD1;
    float3 normalVS    : NORMAL;
    float3 tangentVS   : TANGENT;
    float3 bitangentVS : BINORMAL;
    float3 positionVS  : POSITION;
    float3 positionWS  : POSITION1;
};

uint GetNumLightsInThisTile( uint tileIndex )
{
    uint numLightsInThisTile = 0;
    //uint index = uniforms.maxLightsPerTile * tileIndex;
    uint index = vk::RawBufferLoad < uint > (pushConstants.lightIndexBuf + 4 * uniforms.maxLightsPerTile * tileIndex);
    //uint nextLightIndex = perTileLightIndexBuffer[ index ];
    uint nextLightIndex = vk::RawBufferLoad<uint > (pushConstants.lightIndexBuf + 4 * index);
    
    // count point lights
    while (nextLightIndex != LIGHT_INDEX_BUFFER_SENTINEL)
    {
        ++numLightsInThisTile;
        ++index;
        //nextLightIndex = perTileLightIndexBuffer[ index ];
        nextLightIndex = vk::RawBufferLoad< uint > (pushConstants.lightIndexBuf + 4 * index);
    }

    ++index;
    //nextLightIndex = perTileLightIndexBuffer[ index ];
    nextLightIndex = vk::RawBufferLoad< uint > (pushConstants.lightIndexBuf + 4 * index);
    
    // count spot lights
    while (nextLightIndex != LIGHT_INDEX_BUFFER_SENTINEL)
    {
        ++numLightsInThisTile;
        ++index;
        //nextLightIndex = perTileLightIndexBuffer[ index ];
        nextLightIndex = vk::RawBufferLoad< uint > (pushConstants.lightIndexBuf + 4 * index);
    }

    return numLightsInThisTile;
}

uint GetTileIndex( float2 screenPos )
{
    const float tileRes = (float) TILE_RES;
    uint numCellsX = (uniforms.tilesXY.x + TILE_RES - 1) / TILE_RES; // tilesXY.x is screen width in pixels
    uint tileIdx = floor( screenPos.x / tileRes ) + floor( screenPos.y / tileRes ) * numCellsX;
    return tileIdx;
}

VSOutput standardVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    float2 uv = vk::RawBufferLoad < float2 > (pushConstants.uvBuf + 8 * vertexId);
    vsOut.uv = uv;
    float3 pos = vk::RawBufferLoad < float3 > (pushConstants.posBuf + 12 * vertexId);
    vsOut.pos = mul( uniforms.localToClip, float4( pos, 1 ) );
    float3 normal = vk::RawBufferLoad< float3 > (pushConstants.normalBuf + 12 * vertexId);
    vsOut.normalVS = mul( uniforms.localToView, float4( normal, 0 ) ).xyz;
    float4 tangent = vk::RawBufferLoad< float4 > (pushConstants.tangentBuf + 16 * vertexId);
    vsOut.tangentVS = mul( uniforms.localToView, float4( tangent.xyz, 0 ) ).xyz;
    vsOut.projCoord = mul( uniforms.localToShadowClip, float4( pos, 1 ) );
    vsOut.positionVS = mul( uniforms.localToView, float4( pos, 1 ) ).xyz;
    vsOut.positionWS = mul( uniforms.localToWorld, float4( pos, 1 ) ).xyz;
    
    // aether:
    //float3 ct = cross( tangent.xyz, normal ) * tangent.w;
    // mikkt
    float3 ct = cross( normal, tangent.xyz ) * tangent.w;
    vsOut.bitangentVS = mul( uniforms.localToView, float4( ct, 0 ) ).xyz;

    return vsOut;
}

float linstep( float low, float high, float v )
{
    return saturate( (v - low) / (high - low) );
}

float VSM( float depth, float4 projCoord )
{
    float2 uv = (projCoord.xy / projCoord.w) * 0.5f + 0.5f;
    float2 moments = texture2ds[ pushConstants.shadowTextureIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv, 0 ).rg;
    //if (moments.x > depth && projCoord.w > 0) // projCoord.w > 0 tries to prevent darkening the area outside the shadow map
    //    return 0.2f;
    //return 1.0f;

    float variance = max( moments.y - moments.x * moments.x, -0.001f );

    float delta = depth - moments.x;
    float p = smoothstep( depth - 0.02f, depth, moments.x );
    float minAmbient = 0.2f;
    float pMax = linstep( minAmbient, 1.0f, variance / (variance + delta * delta) );

    return saturate( max( p, pMax ) );
}

float3 tangentSpaceTransform( float3 tangent, float3 bitangent, float3 normal, float3 v )
{
    return normalize( v.x * tangent + v.y * bitangent + v.z * normal );
}

float4 standardPS( VSOutput vsOut ) : SV_Target
{
    float surfaceDistToLight = length( uniforms.lightPosition.xyz - vsOut.positionWS );
    //float depth = (vsOut.projCoord.z + 0.0001f) / vsOut.projCoord.w;
    float shadow = max( 0.2f, VSM( surfaceDistToLight, vsOut.projCoord ) );
    float2 normalTex = texture2ds[ pushConstants.normalMapIndex ].Sample( samplers[ S_LINEAR_REPEAT ], vsOut.uv ).xy;
    float3 normalTS = float3( normalTex.x, normalTex.y, sqrt( 1 - normalTex.x * normalTex.x - normalTex.y * normalTex.y ) );
    float3 normalVS = tangentSpaceTransform( vsOut.tangentVS, vsOut.bitangentVS, vsOut.normalVS, normalTS.xyz );

    const float3 V = normalize( vsOut.positionVS );
    const float3 L = -uniforms.lightDirection.xyz;
    const float3 H = normalize( L + V );
    float specularStrength = 0.5;
    
    float3 ambient = float3( 0.2, 0.2, 0.2 );
    float3 accumDiffuseAndSpecular = uniforms.lightColor.rgb;
    const float3 surfaceToLightVS = mul( uniforms.localToView, uniforms.lightDirection ).xyz;
    float dotNL = saturate( dot( normalVS, -surfaceToLightVS ) );
    accumDiffuseAndSpecular *= dotNL;
    
    float3 reflectDir = reflect( -surfaceToLightVS, normalVS );
    float spec = pow( max( dot( V, reflectDir ), 0.0 ), 32 );
    float3 specular = specularStrength * spec;
    accumDiffuseAndSpecular += specular;
    
    float4 albedo = texture2ds[ pushConstants.textureIndex ].Sample( samplers[ S_LINEAR_REPEAT ], vsOut.uv );
    
    return albedo * float4( saturate( accumDiffuseAndSpecular + ambient ) * shadow, 1 );
}
