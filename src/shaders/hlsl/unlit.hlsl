#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
};

VSOutput unlitVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    vsOut.pos = mul( data.localToClip[ 0 ], float4( positions[ vertexId ], 1 ) );

    return vsOut;
}

float4 unlitFS( VSOutput vsOut ) : SV_Target
{
    return float4( 1, 0, 0, 1 );
}
