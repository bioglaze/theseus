#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float2 uv;
};

struct ImDrawVert
{
    float2  pos;
    float2  uv;
    uint   col;
};

vertex ColorInOut uiVS( uint vid [[ vertex_id ]], const device ImDrawVert* vertices [[ buffer(0) ]] )
{
    ColorInOut out;

    out.position = float4( vertices[ vid ].pos, 1, 1 );
    out.uv = vertices[ vid ].uv;
    
    return out;
}

fragment float4 uiPS( ColorInOut in [[stage_in]], texture2d<float, access::sample> textureMap [[texture(0)]]/*, sampler sampler0 [[sampler(0)]]*/ )
{
    constexpr sampler sampler0( coord::normalized, address::repeat, filter::nearest );
    //return float4( 1, 0, 0, 1 );
    return textureMap.sample( sampler0, in.uv );
}
