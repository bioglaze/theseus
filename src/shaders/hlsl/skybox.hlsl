#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float3 uv : TEXCOORD;
};

VSOutput skyboxVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    vsOut.uv = positions[ vertexId ];
    vsOut.pos = mul( uniforms.localToClip[ 0 ], float4( positions[ vertexId ], 1 ) );

    return vsOut;
}

float4 skyboxPS( VSOutput vsOut ) : SV_Target
{
    return textureCubes[ pushConstants.textureIndex ].Sample( samplers[ 0 ], vsOut.uv );
}
