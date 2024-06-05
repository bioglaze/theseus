#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput momentsVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    vsOut.pos = mul( uniforms.localToClip, float4( positions[ vertexId ], 1 ) );
    vsOut.uv = uvs[ vertexId ];

    return vsOut;
}

float4 momentsPS( VSOutput vsOut ) : SV_Target
{
    float linearDepth = vsOut.pos.z;

    float dx = ddx( linearDepth );
    float dy = ddy( linearDepth );

    float moment1 = linearDepth;
    float moment2 = linearDepth * linearDepth + 0.25f * (dx * dx + dy * dy);

    return float4( moment1, moment2, 0.0f, 1.0f );
}
