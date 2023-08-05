#include "light.h"
#include "te_stdlib.h"
#include "vec3.h"

struct DirectionalLightImpl
{
    Vec3 color;
    unsigned shadowDimension{ 512 };
    bool castShadow{ false };
};

constexpr unsigned MaxLights = 4096;
DirectionalLightImpl dirLights[ MaxLights ];

void teDirectionalLightSetColor( unsigned index, const Vec3& color )
{
    teAssert( index < MaxLights );

    dirLights[ index ].color = color;
}

Vec3 teDirectionalLightGetColor( unsigned index )
{
    teAssert( index < MaxLights );

    return dirLights[ index ].color;
}

void teDirectionalLightSetCastShadow( unsigned index, bool castShadow, unsigned dimension )
{
    teAssert( index < MaxLights );
    teAssert( dimension > 0 );

    dirLights[ index ].castShadow = castShadow;
    dirLights[ index ].shadowDimension = dimension;
}

void teDirectionalLightGetShadowInfo( unsigned index, bool& outCastShadow, unsigned& outDimension )
{
    teAssert( index < MaxLights );

    outCastShadow = dirLights[ index ].castShadow;
    outDimension = dirLights[ index ].shadowDimension;
}
