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
[numthreads(128, 1, 1)]
void unlitMS( uint gtid : SV_GroupThreadID, uint gid : SV_GroupID, out indices uint3 triangles[ 128 ], out vertices VSOutput vertices[ 64 ] )
{
    uint4 meshletData = vk::RawBufferLoad < uint4 > (pushConstants.meshletBuf + 16 * gid);
    
    Meshlet meshlet;
    meshlet.vertexOffset = meshletData.x;
    meshlet.triangleOffset = meshletData.y;
    meshlet.vertexCount = meshletData.z;
    meshlet.triangleCount = meshletData.w;
    
    SetMeshOutputCounts( meshlet.vertexCount, meshlet.triangleCount );

    if (gtid < meshlet.triangleCount)
    {
        uint packed = vk::RawBufferLoad < uint > (pushConstants.meshletIndexBuf + 4 * (meshlet.triangleOffset + gtid));
        uint vIdx0 = (packed >> 0) & 0xFF;
        uint vIdx1 = (packed >> 8) & 0xFF;
        uint vIdx2 = (packed >> 16) & 0xFF;
        
        triangles[ gtid ] = uint3( vIdx0, vIdx1, vIdx2 );
    }
    
    
    if (gtid < meshlet.vertexCount)
    {
        float4 vertexData = vk::RawBufferLoad < float4 > (pushConstants.meshletVertexBuf + 16 * (meshlet.vertexOffset + gtid));
        
        float s = 10;
        vertices[ gtid ].pos = float4( vertexData.x, vertexData.y, vertexData.z, 1.0 );
        vertices[ gtid ].pos = mul( uniforms.localToClip, vertices[ gtid ].pos );
        vertices[ gtid ].uv = float2( 1.0, 0.0 );
    }
}

float4 unlitPS( VSOutput vsOut ) : SV_Target
{
    return texture2ds[ pushConstants.textureIndex ].Sample( samplers[ pushConstants.textureIndex ], vsOut.uv ) * uniforms.tint;
}
