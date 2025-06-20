struct UniformData
{
    matrix localToClip;
    matrix localToView;
    matrix localToShadowClip;
    matrix localToWorld;
    float4 bloomParams;
    float4 tilesXY;
    float4 tint;
    float4 lightDirection;
    float4 lightColor;
    float4 lightPosition;
};

struct PushConstants
{
    uint64_t posBuf;
    uint64_t uvBuf;
    uint64_t normalBuf;
    uint64_t tangentBuf;
    int textureIndex;
    int shadowTextureIndex;
    int normalMapIndex;
    int specularMapIndex;
    float2 scale;
    float2 translate;
};

#define S_LINEAR_REPEAT 0
#define S_LINEAR_CLAMP 1
#define S_NEAREST_REPEAT 2
#define S_NEAREST_CLAMP 3
#define S_ANISO8_REPEAT 4
#define S_ANISO8_CLAMP 5

[[vk::push_constant]] PushConstants pushConstants;

[[vk::binding(0)]] Texture2D<float4> texture2ds[ 80 ];
[[vk::binding(0)]] TextureCube<float4> textureCubes[ 80 ];
[[vk::binding(1)]] SamplerState samplers[ 80 ];
[[vk::binding(2)]] Buffer<float3> positions;
[[vk::binding(3)]] ConstantBuffer< UniformData > uniforms;
[[vk::binding(4)]] Buffer<float2> uvs;
[[vk::binding(5)]] RWTexture2D<float4> rwTexture2d;

