#include <simd/simd.h>

struct Uniforms
{
    matrix_float4x4 localToClip;
    matrix_float4x4 localToView;
    matrix_float4x4 localToShadowClip;
    matrix_float4x4 localToWorld;
    matrix_float4x4 clipToView;
    float4 bloomParams;
    float4 tilesXY;
    float4 tint;
    float4 lightDir;
    float4 lightColor;
    float4 lightPosition;
    uint pointLightCount;
    uint spotLightCount;
    uint maxLightsPerTile;
};
