#include <simd/simd.h>

struct Uniforms
{
    matrix_float4x4 localToClip[ 2 ];
    matrix_float4x4 localToView[ 2 ];
};
