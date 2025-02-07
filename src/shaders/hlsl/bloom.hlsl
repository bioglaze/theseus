#include "ubo.h"

[numthreads( 8, 8, 1 )]
void bloomThreshold( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    const float4 color = texture2ds[ pushConstants.textureIndex ].Load( uint3( globalIdx.x * 1, globalIdx.y * 1, 0 ) );
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
    uv.x = (globalIdx.x + 1) / uniforms.tilesXY.x - 0.5 / uniforms.tilesXY.x;
    uv.y = (globalIdx.y + 1) / uniforms.tilesXY.y - 0.5 / uniforms.tilesXY.y;
    
    const float4 color = texture2ds[ pushConstants.textureIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv, 0 );
    
    rwTexture2d[ globalIdx.xy ] = color;
}

[numthreads( 8, 8, 1 )]
void bloomCombine( uint3 globalIdx : SV_DispatchThreadID, uint3 localIdx : SV_GroupThreadID, uint3 groupIdx : SV_GroupID )
{
    float2 uv;
    uv.x = globalIdx.x / uniforms.tilesXY.x;
    uv.y = globalIdx.y / uniforms.tilesXY.y;
    
    float2 uv2;
    uv2.x = (globalIdx.x * 2 + 1) / (uniforms.tilesXY.x * 2);
    uv2.y = (globalIdx.y * 2 + 1) / (uniforms.tilesXY.y * 2);

    float2 uv3;
    uv3.x = (globalIdx.x * 4 + 1) / (uniforms.tilesXY.x * 4);
    uv3.y = (globalIdx.y * 4 + 1) / (uniforms.tilesXY.y * 4);

    float2 uv4;
    uv4.x = (globalIdx.x * 8 + 1) / (uniforms.tilesXY.x * 8);
    uv4.y = (globalIdx.y * 8 + 1) / (uniforms.tilesXY.y * 8);

    const float4 color1 = texture2ds[ pushConstants.textureIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv, 0 );
    const float4 color2 = texture2ds[ pushConstants.shadowTextureIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv2, 0 );
    const float4 color3 = texture2ds[ pushConstants.normalMapIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv3, 0 );
    const float4 color4 = texture2ds[ pushConstants.specularMapIndex ].SampleLevel( samplers[ S_LINEAR_CLAMP ], uv4, 0 );
    
    float4 accumColor = color1 * uniforms.tint.x;
    accumColor += color2 * uniforms.tint.y;
    accumColor += color3 * uniforms.tint.z;
    accumColor += color4 * uniforms.tint.w;
    
    rwTexture2d[ globalIdx.xy ] = accumColor;
}
