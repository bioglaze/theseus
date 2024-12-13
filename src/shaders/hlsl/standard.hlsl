#include "ubo.h"

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

VSOutput standardVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    vsOut.uv = uvs[ vertexId  ];
    vsOut.pos = mul( uniforms.localToClip, float4( positions[ vertexId ], 1 ) );
    vsOut.normalVS = mul( uniforms.localToView, float4( normals[ vertexId ], 0 ) ).xyz;
    vsOut.tangentVS = mul( uniforms.localToView, float4( tangents[ vertexId ].xyz, 0 ) ).xyz;
    vsOut.projCoord = mul( uniforms.localToShadowClip, float4( positions[ vertexId ], 1 ) );
    vsOut.positionVS = mul( uniforms.localToView, float4( positions[ vertexId ], 1 ) ).xyz;
    vsOut.positionWS = mul( uniforms.localToWorld, float4( positions[ vertexId ], 1 ) ).xyz;
    
    // aether:
    //float3 ct = cross( tangents[ vertexId ].xyz, normals[ vertexId ] ) * tangents[ vertexId ].w;
    // mikkt
    float3 ct = cross( normals[ vertexId ], tangents[ vertexId ].xyz ) * tangents[ vertexId ].w;
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
    float2 normalTex = texture2ds[ pushConstants.normalMapIndex ].Sample( samplers[ 0 ], vsOut.uv ).xy;
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
    
    float4 albedo = texture2ds[ pushConstants.textureIndex ].Sample( samplers[ 0 ], vsOut.uv );
    
    return albedo * float4( saturate( accumDiffuseAndSpecular + ambient ) * shadow, 1 );
}
