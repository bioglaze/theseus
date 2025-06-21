#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float3 uv : TEXCOORD;
};

VSOutput skyboxVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    float3 pos = vk::RawBufferLoad< float3 > (pushConstants.posBuf + 12 * vertexId);
    vsOut.uv = pos;
    vsOut.pos = mul( uniforms.localToClip, float4( pos, 1 ) );

    return vsOut;
}

float4 skyboxPS( VSOutput vsOut ) : SV_Target
{
    return textureCubes[ pushConstants.textureIndex ].Sample( samplers[ 0 ], vsOut.uv );
}
