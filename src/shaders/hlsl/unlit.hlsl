#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput unlitVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    vsOut.pos = mul( uniforms.localToClip, float4( positions[ vertexId ], 1 ) );
    //float4 vTest = vk::RawBufferLoad<float4>( pushConstants.posBuf + 16 );
    vsOut.uv = uvs[ vertexId ];

    return vsOut;
}

float4 unlitPS( VSOutput vsOut ) : SV_Target
{
    return texture2ds[ pushConstants.textureIndex ].Sample( samplers[ pushConstants.textureIndex ], vsOut.uv ) * uniforms.tint;
}
