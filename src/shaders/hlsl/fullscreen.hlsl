#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput fullscreenVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    vsOut.pos = mul( uniforms.localToClip, float4( positions[ vertexId ], 1 ) );
    vsOut.pos.xy *= uniforms.tilesXY.xy;
    vsOut.pos.xy += uniforms.tilesXY.wz;
    vsOut.uv = uvs[ vertexId ];
    
    return vsOut;
}

float4 fullscreenPS( VSOutput vsOut ) : SV_Target
{
    return texture2ds[ pushConstants.textureIndex ].Sample( samplers[ 0 ], vsOut.uv );
}

float4 fullscreenAdditivePS(VSOutput vsOut) : SV_Target
{
    return texture2ds[ pushConstants.textureIndex ].Sample( samplers[ 0 ], vsOut.uv ).rrrr;
}
