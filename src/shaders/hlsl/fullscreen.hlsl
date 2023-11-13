#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput fullscreenVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    vsOut.uv = float2( (vertexId << 1) & 2, vertexId & 2 );
    vsOut.pos = float4( vsOut.uv * 2.0f + -1.0f, 0.0f, 1.0f );
    
    return vsOut;
}

float4 fullscreenPS( VSOutput vsOut ) : SV_Target
{
    return texture2ds[ pushConstants.textureIndex ].Sample( samplers[ 0 ], vsOut.uv );
}
