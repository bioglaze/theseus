#include "material.h"
#include "shader.h"
#include "texture.h"
#include "vec3.h"

struct MaterialImpl
{
    teTexture2D texture2Ds[ 3 ];
    teShader shader;
    Vec4 tint{ 1, 1, 1, 1 };
};

static constexpr unsigned MaxMaterial = 500;
MaterialImpl materials[ MaxMaterial ];
unsigned materialCount = 0;

teMaterial teCreateMaterial( const teShader& shader )
{
    teMaterial outMaterial;
    outMaterial.index = materialCount++;
    
    materials[ outMaterial.index ].shader = shader;
    
    return outMaterial;
}

void teMaterialSetTexture2D( const teMaterial& material, const struct teTexture2D& tex, unsigned slot )
{
    if (slot < 3)
    {
        materials[ material.index ].texture2Ds[ slot ] = tex;
    }
}

teShader& teMaterialGetShader( const teMaterial& material )
{
    return materials[ material.index ].shader;
}

teTexture2D teMaterialGetTexture2D( const teMaterial& material, unsigned slot )
{
    return materials[ material.index ].texture2Ds[ slot ];
}

void teMaterialSetTint( const teMaterial& material, Vec4 tint )
{
    materials[ material.index ].tint = tint;
}

Vec4 teMaterialGetTint( const teMaterial& material )
{
    return materials[ material.index ].tint;
}
