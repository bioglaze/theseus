#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
    float4 color : COLOR;
};

// for reference
/*struct ImDrawVert
{
    ImVec2  pos;
    ImVec2  uv;
    ImU32   col;
};*/

VSOutput uiVS( uint vertexId : SV_VertexID, float2 pos : POSITION, float2 uv : TEXCOORD, float4 color : COLOR )
{
    VSOutput vsOut;
    vsOut.pos = float4( pos * pushConstants.scale + pushConstants.translate, 0, 1 );
    vsOut.uv = uv;
    vsOut.color = color;

    return vsOut;
}

float4 uiPS( VSOutput vsOut ) : SV_Target
{
    return vsOut.color * texture2ds[ pushConstants.textureIndex ].Sample( samplers[ pushConstants.textureIndex ], vsOut.uv );
}
