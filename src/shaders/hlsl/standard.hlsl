#include "ubo.h"

struct VSOutput
{
    float4 pos         : SV_Position;
    float2 uv          : TEXCOORD0;
    float4 projCoord   : TEXCOORD1;
    float3 normalVS    : NORMAL;
    float3 tangentVS   : TANGENT;
    float3 bitangentVS : BINORMAL;
};

VSOutput standardVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    vsOut.uv = uvs[ vertexId  ];
    vsOut.pos = mul( uniforms.localToClip, float4( positions[ vertexId ], 1 ) );
    vsOut.normalVS = mul( uniforms.localToView, float4( normals[ vertexId ], 0 ) ).xyz;
    vsOut.tangentVS = mul( uniforms.localToView, float4( tangents[ vertexId ].xyz, 0 ) ).xyz;
    vsOut.projCoord = mul( uniforms.localToShadowClip, float4( positions[ vertexId ], 1 ) );

    float3 ct = cross( tangents[ vertexId ].xyz, normals[ vertexId ] ) * tangents[ vertexId ].w;
    vsOut.bitangentVS = mul( uniforms.localToView, float4( ct, 0 ) ).xyz;

    return vsOut;
}

float linstep( float low, float high, float v )
{
    return saturate( (v - low) / (high - low) );
}

float VSM( float depth, float4 projCoord )
{
    float2 uv = (projCoord.xy / projCoord.w);// * 0.5f + 0.5f; // FIXME: Why was this scale/bias applied? Vulkan z-clip range is 0-1, not -1-1.
    float2 moments = texture2ds[ pushConstants.shadowTextureIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv, 0 ).rg;
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

float3 tangentSpaceTransform( float3 tangent, float3 bitangent, float3 normal, float3 v )
{
    return normalize( v.x * tangent + v.y * bitangent + v.z * normal );
}

float4 standardPS( VSOutput vsOut ) : SV_Target
{
    float depth = (vsOut.projCoord.z + 0.0001f) / vsOut.projCoord.w;
    float shadow = max( 0.2f, VSM( depth, vsOut.projCoord ) );
    float2 normalTex = texture2ds[ pushConstants.normalMapIndex ].Sample( samplers[ 0 ], vsOut.uv ).xy;
    float3 normalTS = float3( normalTex.x, normalTex.y, sqrt( 1 - normalTex.x * normalTex.x - normalTex.y * normalTex.y ) );
    float3 normalVS = tangentSpaceTransform( vsOut.tangentVS, vsOut.bitangentVS, vsOut.normalVS, normalTS.xyz );
    
    float3 accumDiffuseAndSpecular = uniforms.lightColor.rgb;
    const float3 surfaceToLightVS = mul( uniforms.localToView, uniforms.lightDirection ).xyz;
    float dotNL = saturate( dot( normalVS, surfaceToLightVS ) );
    accumDiffuseAndSpecular *= dotNL;
    
    return texture2ds[ pushConstants.textureIndex ].Sample( samplers[ 0 ], vsOut.uv ) * float4( accumDiffuseAndSpecular, 1 );// * shadow;
}
