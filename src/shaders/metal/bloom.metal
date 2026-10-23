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
    if ( gid.x >= (uint)uniforms.tilesXY.x || gid.y >= (uint)uniforms.tilesXY.y )
        return;

    const float4 color = colorTexture.read( gid );
    const float luminance = dot( color.rgb, float3( 0.2126f, 0.7152f, 0.0722f ) );
    const float luminanceThreshold = uniforms.bloomParams.w;
    const float4 finalColor = luminance > luminanceThreshold ? color : float4( 0, 0, 0, 0 );
    downsampledBrightTexture.write( finalColor, gid.xy );
}

kernel void bloomBlur(texture2d<float, access::read> inputTexture [[texture(0)]],
                  texture2d<float, access::write> resultTexture [[texture(1)]],
                  constant Uniforms& uniforms [[ buffer(0) ]],
                  ushort2 gid [[thread_position_in_grid]],
                  ushort2 tid [[thread_position_in_threadgroup]],
                  ushort2 dtid [[threadgroup_position_in_grid]])
{
    if ( gid.x >= (uint)uniforms.tilesXY.x || gid.y >= (uint)uniforms.tilesXY.y )
        return;

    const int2 texel = int2( gid.xy );
    const int2 maxTexel = int2( (int) uniforms.tilesXY.x, (int) uniforms.tilesXY.y ) - 1;
    const int2 blurStep = int2( (int) uniforms.tilesXY.z, (int) uniforms.tilesXY.w );

    float weights[ 5 ] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };
    float4 accumColor = inputTexture.read( uint2( gid.x, gid.y ) ) * weights[ 0 ];

    for (int x = 1; x < 5; ++x)
    {
        const int2 tap = blurStep * x;
        // The taps are clamped to the texture, otherwise the ones that reach outside
        // of it would wrap around and read from the opposite side.
        const uint3 tapPlus = uint3( uint( clamp( texel.x + tap.x, 0, maxTexel.x ) ), uint( clamp( texel.y + tap.y, 0, maxTexel.y ) ), 0 );
        const uint3 tapMinus = uint3( uint( clamp( texel.x - tap.x, 0, maxTexel.x ) ), uint( clamp( texel.y - tap.y, 0, maxTexel.y ) ), 0 );

        accumColor += inputTexture.read( tapPlus.xy ) * weights[ x ];
        accumColor += inputTexture.read( tapMinus.xy ) * weights[ x ];
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
    if ( gid.x >= (uint)uniforms.tilesXY.x || gid.y >= (uint)uniforms.tilesXY.y )
        return;

    constexpr sampler sampler0( coord::normalized, address::clamp_to_zero, filter::linear );

    float2 uv;
    uv.x = (gid.x + 0.5) / uniforms.tilesXY.x;
    uv.y = (gid.y + 0.5) / uniforms.tilesXY.y;

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
    if ( gid.x >= (uint)uniforms.tilesXY.x || gid.y >= (uint)uniforms.tilesXY.y )
        return;

    constexpr sampler sampler0( coord::normalized, address::clamp_to_zero, filter::linear );

    const float2 uv = ( float2( gid.xy ) + 0.5 ) / uniforms.tilesXY.xy;

    const float4 color1 = bloomTexture.sample( sampler0, uv, 0 );
    const float4 color2 = downsampleTexture1.sample( sampler0, uv, 0 );
    const float4 color3 = downsampleTexture2.sample( sampler0, uv, 0 );
    const float4 color4 = downsampleTexture3.sample( sampler0, uv, 0 );
    
    float4 accumColor = color1 * uniforms.tint.x;
    accumColor += color2 * uniforms.tint.y;
    accumColor += color3 * uniforms.tint.z;
    accumColor += color4 * uniforms.tint.w;
    
    resultTexture.write( accumColor, gid.xy );
}
