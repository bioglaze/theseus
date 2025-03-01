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
    constexpr sampler sampler0( coord::normalized, address::clamp_to_zero, filter::linear );

    float2 uv;
    uv.x = (gid.x * 2 + 0.5) / uniforms.tilesXY.x;
    uv.y = (gid.y * 2 + 0.5) / uniforms.tilesXY.y;

    const float4 color = colorTexture.sample( sampler0, uv, 0 );
    
    resultTexture.write( color, gid.xy );
}

kernel void bloomCombine(texture2d<float, access::sample> bloomTexture [[texture(0)]],
                  texture2d<float, access::write> resultTexture [[texture(1)]],
                  texture2d<float, access::sample> downsampleTexture1 [[texture(2)]],
                  texture2d<float, access::sample> downsampleTexture2 [[texture(3)]],
                  texture2d<float, access::sample> downsampleTexture3 [[texture(4)]],
                  constant Uniforms& uniforms [[ buffer(0) ]],
                  ushort2 gid [[thread_position_in_grid]],
                  ushort2 tid [[thread_position_in_threadgroup]],
                  ushort2 dtid [[threadgroup_position_in_grid]])
{
    constexpr sampler sampler0( coord::normalized, address::clamp_to_zero, filter::linear );

    float2 uv;
    uv.x = gid.x / uniforms.tilesXY.x;
    uv.y = gid.y / uniforms.tilesXY.y;
    
    float2 uv2;
    uv2.x = (gid.x * 1 + 1) / (uniforms.tilesXY.x * 2);
    uv2.y = (gid.y * 1 + 1) / (uniforms.tilesXY.y * 2);

    float2 uv3;
    uv3.x = (gid.x * 1 + 1) / (uniforms.tilesXY.x * 4);
    uv3.y = (gid.y * 1 + 1) / (uniforms.tilesXY.y * 4);

    float2 uv4;
    uv4.x = (gid.x * 1 + 1) / (uniforms.tilesXY.x * 8);
    uv4.y = (gid.y * 1 + 1) / (uniforms.tilesXY.y * 8);

    const float4 color1 = bloomTexture.sample( sampler0, uv, 0 );
    const float4 color2 = downsampleTexture1.sample( sampler0, uv2, 0 );
    const float4 color3 = downsampleTexture2.sample( sampler0, uv3, 0 );
    const float4 color4 = downsampleTexture3.sample( sampler0, uv4, 0 );
    
    float4 accumColor = color1 * uniforms.tint.x;
    accumColor += color2 * uniforms.tint.y;
    accumColor += color3 * uniforms.tint.z;
    accumColor += color4 * uniforms.tint.w;
    
    resultTexture.write( accumColor, gid.xy );
}
