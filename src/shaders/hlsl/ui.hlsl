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

VSOutput uiVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    float2 posu = vk::RawBufferLoad< float2 > (pushConstants.posBuf + 20 * vertexId); // 20 = sizeof( ImDrawVert )
    vsOut.pos = float4( posu * pushConstants.scale + pushConstants.translate, 0, 1 );
    
    float2 uvu = vk::RawBufferLoad < float2 > (pushConstants.posBuf + 20 * vertexId + 8);
    vsOut.uv = uvu;
    
    uint cole = vk::RawBufferLoad < uint > (pushConstants.posBuf + 20 * vertexId + 16);
    uint colr = cole >> 24;
    uint colg = (cole & 0x00ff0000) >> 16;
    uint colb = (cole & 0x0000ff00) >> 8;
    uint cola = (cole & 0x000000ff);
    
    vsOut.color = float4( ((float) cola) / 255, ((float) colb) / 255, ((float) colg) / 255, ((float) colr) / 255 );

    return vsOut;
}

float4 uiPS( VSOutput vsOut ) : SV_Target
{
    return vsOut.color * texture2ds[ pushConstants.textureIndex ].Sample( samplers[ pushConstants.textureIndex ], vsOut.uv );
}
