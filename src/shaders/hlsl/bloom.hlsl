#include "ubo.h"

[numthreads( 8, 8, 1 )]
void bloomThreshold( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    if ( globalIdx.x >= (uint) uniforms.tilesXY.x || globalIdx.y >= (uint) uniforms.tilesXY.y )
        return;

    const float4 color = texture2ds[ pushConstants.textureIndex ].Load( uint3( globalIdx.x, globalIdx.y, 0 ) );
    const float luminance = dot( color.rgb, float3( 0.2126f, 0.7152f, 0.0722f ) );
    const float luminanceThreshold = uniforms.bloomParams.w;
    const float4 finalColor = luminance > luminanceThreshold ? color : float4( 0, 0, 0, 0 );

    rwTexture2d[ globalIdx.xy ] = finalColor;
}

[numthreads( 8, 8, 1 )]
void bloomBlur( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    if ( globalIdx.x >= (uint) uniforms.tilesXY.x || globalIdx.y >= (uint) uniforms.tilesXY.y )
        return;

    const int2 texel = int2( globalIdx.xy );
    const int2 maxTexel = int2( (int) uniforms.tilesXY.x, (int) uniforms.tilesXY.y ) - 1;
    const int2 blurStep = int2( (int) uniforms.tilesXY.z, (int) uniforms.tilesXY.w );

    float weights[ 5 ] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };
    float4 accumColor = texture2ds[ pushConstants.textureIndex ].Load( uint3( uint( texel.x ), uint( texel.y ), 0 ) ) * weights[ 0 ];

    for (int x = 1; x < 5; ++x)
    {
        const int2 tap = blurStep * x;
        // The taps are clamped to the texture, otherwise the ones that reach outside
        // of it would wrap around and read from the opposite side.
        const uint3 tapPlus = uint3( uint( clamp( texel.x + tap.x, 0, maxTexel.x ) ), uint( clamp( texel.y + tap.y, 0, maxTexel.y ) ), 0 );
        const uint3 tapMinus = uint3( uint( clamp( texel.x - tap.x, 0, maxTexel.x ) ), uint( clamp( texel.y - tap.y, 0, maxTexel.y ) ), 0 );
        accumColor += texture2ds[ pushConstants.textureIndex ].Load( tapPlus ) * weights[ x ];
        accumColor += texture2ds[ pushConstants.textureIndex ].Load( tapMinus ) * weights[ x ];
    }

    rwTexture2d[ globalIdx.xy ] = accumColor;
}

[numthreads( 8, 8, 1 )]
void bloomDownsample( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    if ( globalIdx.x >= (uint) uniforms.tilesXY.x || globalIdx.y >= (uint) uniforms.tilesXY.y )
        return;

    const float2 uv = ( float2( globalIdx.xy ) + 0.5 ) / uniforms.tilesXY.xy;

    const float4 color = texture2ds[ pushConstants.textureIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv, 0 );

    rwTexture2d[ globalIdx.xy ] = color;
}

[numthreads( 8, 8, 1 )]
void bloomCombine( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    if ( globalIdx.x >= (uint) uniforms.tilesXY.x || globalIdx.y >= (uint) uniforms.tilesXY.y )
        return;

    const float2 uv = ( float2( globalIdx.xy ) + 0.5 ) / uniforms.tilesXY.xy;

    const float4 color1 = texture2ds[ pushConstants.textureIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv, 0 );
    const float4 color2 = texture2ds[ pushConstants.shadowTextureIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv, 0 );
    const float4 color3 = texture2ds[ pushConstants.normalMapIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv, 0 );
    const float4 color4 = texture2ds[ pushConstants.specularMapIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv, 0 );

    float4 accumColor = color1 * uniforms.tint.x;
    accumColor += color2 * uniforms.tint.y;
    accumColor += color3 * uniforms.tint.z;
    accumColor += color4 * uniforms.tint.w;

    rwTexture2d[ globalIdx.xy ] = accumColor;
}
