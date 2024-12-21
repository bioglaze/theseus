#include "ubo.h"

[numthreads( 8, 8, 1 )]
void bloomThreshold( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    const float4 color = texture2ds[ pushConstants.textureIndex ].Load( uint3( globalIdx.x * 2, globalIdx.y * 2, 0 ) );
    const float luminance = dot( color.rgb, float3( 0.2126f, 0.7152f, 0.0722f ) );
    const float luminanceThreshold = uniforms.bloomParams.w;
    const float4 finalColor = luminance > luminanceThreshold ? color : float4( 0, 0, 0, 0 );

    rwTexture2d[ globalIdx.xy ] = finalColor;
}

[numthreads( 8, 8, 1 )]
void bloomBlur( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    float weights[ 5 ] = { 0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216 };
    float4 accumColor = texture2ds[ pushConstants.textureIndex ].Load( uint3(globalIdx.x, globalIdx.y, 0) ) * weights[ 0 ];
    //accumColor = float4( 1, 0, 0, 1 );
    for (int x = 1; x < 5; ++x)
    {
        accumColor += texture2ds[ pushConstants.textureIndex ].Load( uint3( globalIdx.x + x * uniforms.tilesXY.z, globalIdx.y + x * uniforms.tilesXY.w, 0 ) ) * weights[ x ];
        accumColor += texture2ds[ pushConstants.textureIndex ].Load( uint3( globalIdx.x - x * uniforms.tilesXY.z, globalIdx.y - x * uniforms.tilesXY.w, 0 ) ) * weights[ x ];
    }

    rwTexture2d[ globalIdx.xy ] = accumColor;
}

[numthreads( 8, 8, 1 )]
void bloomDownsample( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    float2 uv;
    uv.x = (globalIdx.x * 2 + 1) / uniforms.tilesXY.x;
    uv.y = (globalIdx.y * 2 + 1) / uniforms.tilesXY.y;
    const float4 color = texture2ds[ pushConstants.textureIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv, 0 );
    
    rwTexture2d[ globalIdx.xy ] = color;
}
