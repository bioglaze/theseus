#include "light.h"
#include "buffer.h"

teBuffer CreateBuffer( unsigned size, const char* debugName );

constexpr unsigned MaxLights = 2048;

teBuffer gPointLightCenterAndRadiusBuffer;

void InitLightTiler()
{
    gPointLightCenterAndRadiusBuffer = CreateBuffer( MaxLights * 4 * sizeof( float ), "gPointLightCenterAndRadiusBuffer" );
}
