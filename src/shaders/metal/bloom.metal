#include <metal_stdlib>
#include <metal_atomic>
#include <simd/simd.h>

#include "ubo.h"

using namespace metal;

kernel void bloomThreshold(texture2d<float, access::read> colorTexture [[texture(0)]],
                  texture2d<float, access::write> downsampledBrightTexture [[texture(1)]],
                  constant Uniforms& uniforms [[ buffer(0) ]],
                  ushort2 gid [[thread_position_in_grid]],
                  ushort2 tid [[thread_position_in_threadgroup]],
                  ushort2 dtid [[threadgroup_position_in_grid]])
{
    const float4 color = colorTexture.read( gid );
    const float luminance = dot( color.rgb, float3( 0.2126f, 0.7152f, 0.0722f ) );
    const float luminanceThreshold = uniforms.bloomParams.w;
    const float4 finalColor = luminance > luminanceThreshold ? color : float4( 0, 0, 0, 0 );
    downsampledBrightTexture.write( finalColor, gid.xy / 2 );
}

kernel void bloomBlur(texture2d<float, access::read> inputTexture [[texture(0)]],
                  texture2d<float, access::write> resultTexture [[texture(1)]],
                  constant Uniforms& uniforms [[ buffer(0) ]],
                  ushort2 gid [[thread_position_in_grid]],
                  ushort2 tid [[thread_position_in_threadgroup]],
                  ushort2 dtid [[threadgroup_position_in_grid]])
{
    float weights[ 5 ] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };
    float4 accumColor = inputTexture.read( uint2( gid.x, gid.y ) ) * weights[ 0 ];

    for (int x = 1; x < 5; ++x)
    {
        accumColor += inputTexture.read( uint2( gid.x + x * uniforms.tilesXY.z, gid.y + x * uniforms.tilesXY.w ) ) * weights[ x ];
        accumColor += inputTexture.read( uint2( gid.x - x * uniforms.tilesXY.z, gid.y - x * uniforms.tilesXY.w ) ) * weights[ x ];
    }

    resultTexture.write( accumColor, gid.xy );
}

kernel void bloomDownsample(texture2d<float, access::sample> colorTexture [[texture(0)]],
                  texture2d<float, access::write> resultTexture [[texture(1)]],
                  constant Uniforms& uniforms [[ buffer(0) ]],
                  ushort2 gid [[thread_position_in_grid]],
                  ushort2 tid [[thread_position_in_threadgroup]],
                  ushort2 dtid [[threadgroup_position_in_grid]])
{
    constexpr sampler sampler0( coord::normalized, address::repeat, filter::nearest );

    float2 uv;
    uv.x = (gid.x * 2 + 1) / uniforms.tilesXY.x;
    uv.y = (gid.y * 2 + 1) / uniforms.tilesXY.y;
    const float4 color = colorTexture.sample( sampler0, uv, 0 );
    
    resultTexture.write( color, gid.xy );
}
