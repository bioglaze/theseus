#include <metal_stdlib>
#include <simd/simd.h>
#include "ubo.h"

using namespace metal;

struct ColorInOut
{
    float4 position [[position]];
    float2 uv;
    float4 color;
};

struct ImDrawVert
{
    float2  pos;
    float2  uv;
    uchar4  col;
};

vertex ColorInOut uiVS( uint vid [[ vertex_id ]], const device ImDrawVert* vertices [[ buffer(0) ]], constant Uniforms& uniforms [[ buffer(1) ]] )
{
    ColorInOut out;

    //float4 scaleTrans = uniforms.localToClip[ 0 ][ 0 ];
    //float2 translate = float2( uniforms.localToClip[ 0 ][ 0 ], uniforms.localToClip[ 0 ][ 1 ] );
    //out.position = float4( vertices[ vid ].pos * scaleTrans.xy + scaleTrans.zw, 0, 1 );
    
    out.position = uniforms.localToClip[ 0 ] * float4( vertices[ vid ].pos, 0, 1 );
    out.uv = vertices[ vid ].uv;
    out.color = float4( vertices[ vid ].col ) / float4( 255.0f );
    
    return out;
}

fragment float4 uiPS( ColorInOut in [[stage_in]], texture2d<float, access::sample> textureMap [[texture(0)]]/*, sampler sampler0 [[sampler(0)]]*/ )
{
    constexpr sampler sampler0( coord::normalized, address::repeat, filter::nearest );
    //return float4( 1, 0, 0, 1 );
    return in.color * textureMap.sample( sampler0, in.uv );
}
