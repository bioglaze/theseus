#include "material.h"
#include "shader.h"
#include "texture.h"

struct MaterialImpl
{
    int texture2Ds[ 3 ];
    teShader shader;
};

static constexpr unsigned MaxMaterial = 500;
MaterialImpl materials[ MaxMaterial ];
unsigned materialCount = 0;

teMaterial teCreateMaterial( const teShader& shader )
{
    teMaterial outMaterial;
    outMaterial.index = materialCount++;
    
    materials[ outMaterial.index ].shader = shader;
    materials[ outMaterial.index ].texture2Ds[ 0 ] = 0;
    materials[ outMaterial.index ].texture2Ds[ 1 ] = 0;
    materials[ outMaterial.index ].texture2Ds[ 2 ] = 0;
    
    return outMaterial;
}

void teMaterialSetTexture2D( const teMaterial& material, const struct teTexture2D& tex, unsigned slot )
{
    if (slot < 3)
    {
        materials[ material.index ].texture2Ds[ slot ] = tex.index;
    }
}

teShader& teMaterialGetShader( const teMaterial& material )
{
    return materials[ material.index ].shader;
}

int teMaterialGetTexture2D( const teMaterial& material, unsigned slot )
{
    return materials[ material.index ].texture2Ds[ slot ];
}
