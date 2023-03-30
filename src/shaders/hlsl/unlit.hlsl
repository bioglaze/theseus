#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput unlitVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    vsOut.pos = mul( uniforms.localToClip[ 0 ], float4( positions[ vertexId ], 1 ) );
    vsOut.uv = uvs[ vertexId ];

    return vsOut;
}

float4 unlitPS( VSOutput vsOut ) : SV_Target
{
    return texture2ds[ pushConstants.textureIndex ].Sample( samplers[ pushConstants.samplerIndex ], vsOut.uv );
}
