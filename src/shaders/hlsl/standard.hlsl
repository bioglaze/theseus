#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
    float4 projCoord : TANGENT;
};

VSOutput standardVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    vsOut.uv = uvs[ vertexId  ];
    vsOut.pos = mul(uniforms.localToClip, float4( positions[ vertexId ], 1));

    vsOut.projCoord = mul( uniforms.localToShadowClip, float4( positions[ vertexId ], 1 ) );

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

float4 standardPS( VSOutput vsOut ) : SV_Target
{
    float depth = (vsOut.projCoord.z + 0.0001f) / vsOut.projCoord.w;
    float shadow = max( 0.2f, VSM( depth, vsOut.projCoord ) );
    //float2 normalTex = texture2ds[ pushConstants.normalMapIndex ].Sample( samplers[ 0 ], vsOut.uv ).xy;
    //float3 normalTS = float3( normalTex.x, normalTex.y, sqrt( 1 - normalTex.x * normalTex.x - normalTex.y * normalTex.y ) );

    return texture2ds[ pushConstants.textureIndex ].Sample( samplers[ 0 ], vsOut.uv ) * shadow;
}
