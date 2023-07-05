#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

// for reference
/*struct ImDrawVert
{
    ImVec2  pos;
    ImVec2  uv;
    ImU32   col;
};*/

VSOutput uiVS( uint vertexId : SV_VertexID, float2 pos : POSITION, float2 uv : TEXCOORD, uint color : COLOR )
{
    VSOutput vsOut;
    vsOut.pos = mul( uniforms.localToClip[ 0 ], float4( pos, 0, 1 ) );
    vsOut.uv = uv;

    return vsOut;
}

float4 uiPS( VSOutput vsOut ) : SV_Target
{
    return texture2ds[ pushConstants.textureIndex ].Sample( samplers[ pushConstants.textureIndex ], vsOut.uv );
}
