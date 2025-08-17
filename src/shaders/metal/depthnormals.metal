#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float3 positionVS;
    float3 normalVS;
    float2 uv;
};

vertex ColorInOut depthNormalsVS( uint vid [[ vertex_id ]],
                          constant Uniforms & uniforms [[ buffer(0) ]],
                          const device packed_float3* positions [[ buffer(1) ]],
                          const device packed_float2* uvs [[ buffer(2) ]],
                          const device packed_float3* normals [[ buffer(3) ]] )
{
    ColorInOut out;

    out.position = uniforms.localToClip * float4( positions[ vid ], 1 );
    out.positionVS = (uniforms.localToView * float4( positions[ vid ], 1 )).xyz;
    out.normalVS = (uniforms.localToView * float4( normals[ vid ], 0 )).xyz;
    out.uv = float2( uvs[ vid ] );
    
    return out;
}

fragment float4 depthNormalsPS( ColorInOut in [[stage_in]], texture2d<float, access::sample> textureMap [[texture(0)]],
                        constant Uniforms & uniforms [[ buffer(0) ]]/*, sampler sampler0 [[sampler(0)]]*/ )
{
    return float4( in.positionVS.z, in.normalVS );
}
