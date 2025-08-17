#include "ubo.h"

struct VSOutput
{
    float4 pos : SV_Position;
    float2 uv : TEXCOORD;
    float3 positionVS : POSITION; // OPT: only need .z.
    float3 normalVS : NORMAL;
};

VSOutput depthNormalsVS( uint vertexId : SV_VertexID )
{
    VSOutput vsOut;
    
    float3 pos = vk::RawBufferLoad< float3 > (pushConstants.posBuf + 12 * vertexId);
    vsOut.pos = mul( uniforms.localToClip, float4( pos, 1 ) );
    vsOut.positionVS = mul( uniforms.localToView, float4( pos, 1 ) ).xyz;
    
    float3 normal = vk::RawBufferLoad < float3 > (pushConstants.normalBuf + 12 * vertexId);
    vsOut.normalVS = mul( uniforms.localToView, float4( normal, 0 ) ).xyz;
    
    float2 uv = vk::RawBufferLoad< float2 > (pushConstants.uvBuf + 8 * vertexId);
    vsOut.uv = uv;

    return vsOut;
}

float4 depthNormalsPS( VSOutput vsOut ) : SV_Target
{
    float linearDepth = vsOut.positionVS.z;
    return float4( linearDepth, vsOut.normalVS );
}
