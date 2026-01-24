#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
};

VSOutput unlitVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    float3 pos = vk::RawBufferLoad< float3 > (pushConstants.posBuf + 12 * vertexId);
    vsOut.pos = mul( uniforms.localToClip, float4( pos, 1 ) );
    float2 uv = vk::RawBufferLoad< float2 > (pushConstants.uvBuf + 8 * vertexId);
    vsOut.uv = uv;

    return vsOut;
}

[outputtopology("triangle")]
[numthreads(1, 1, 1)]
void unlitMS( uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out indices uint3 triangles[ 1 ], out vertices VSOutput vertices[ 3 ] )
{
    uint4 meshletData = vk::RawBufferLoad < uint4 > (pushConstants.meshletBuf + 16 * gid);
    
    Meshlet meshlet;
    meshlet.vertexOffset = meshletData.x;
    meshlet.triangleOffset = meshletData.y;
    meshlet.vertexCount = meshletData.z;
    meshlet.triangleCount = meshletData.w;
    
    SetMeshOutputCounts( 3, 1 ); // 3 vertices, 1 primitive

    if (gtid < meshlet.triangleCount)
    {
        triangles[ gtid ] = uint3( 0, 1, 2 );
    }
    
    if (gtid < meshlet.vertexCount)
    {
        float s = 10;
        vertices[ gtid ].pos = float4( -0.5 * s, 0.5 * s, 0.0, 1.0 );
    //vertices[ gtid ].pos = mul( uniforms.localToClip, vertices[ 0 ].pos );
        vertices[ gtid ].uv = float2( 1.0, 0.0 );

        /*vertices[ 1 ].pos = float4( 0.5 * s, 0.5 * s, 0.0, 1.0 );
    //vertices[ 1 ].pos = mul( uniforms.localToClip, vertices[ 1 ].pos );
        vertices[ 1 ].uv = float2( 0.0, 1.0 );

        vertices[ 2 ].pos = float4( 0.0, -0.5 * s, 0.0, 1.0 );
    //vertices[ 2 ].pos = mul( uniforms.localToClip, vertices[ 2 ].pos );
        vertices[ 2 ].uv = float2( 0.0, 0.0 );*/
    }
}

float4 unlitPS( VSOutput vsOut ) : SV_Target
{
    return texture2ds[ pushConstants.textureIndex ].Sample( samplers[ pushConstants.textureIndex ], vsOut.uv ) * uniforms.tint;
}
