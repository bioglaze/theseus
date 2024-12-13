#include "ubo.h"

struct VSOutput
{
    float4 pos   : SV_Position;
    float4 posVS : POSITION;
    float2 uv    : TEXCOORD;
};

VSOutput momentsVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    vsOut.pos = mul( uniforms.localToClip, float4( positions[ vertexId ], 1 ) );
    vsOut.posVS = mul( uniforms.localToView, float4( positions[ vertexId ], 1 ) );
    vsOut.uv = uvs[ vertexId ];

    return vsOut;
}

float4 momentsPS( VSOutput vsOut ) : SV_Target
{
    float distToLight = length( vsOut.posVS );

    float dx = ddx( distToLight );
    float dy = ddy( distToLight );

    float moment1 = distToLight;
    float moment2 = distToLight * distToLight + 0.25f * (dx * dx + dy * dy);

    return float4( moment1, moment2, 0.0f, 1.0f );
}
