#include <simd/simd.h>

struct Uniforms
{
    matrix_float4x4 localToClip;
    matrix_float4x4 localToView;
    matrix_float4x4 localToShadowClip;
    float4 bloomParams;
    float4 tilesXY;
    float4 tint;
};
