#pragma once

// \param color Range is [0, 1]
void teDirectionalLightSetColor( unsigned index, const struct Vec3& color );
Vec3 teDirectionalLightGetColor( unsigned index );
void teDirectionalLightSetCastShadow( unsigned index, bool castShadow, unsigned dimension );
void teDirectionalLightGetShadowInfo( unsigned index, bool& outCastShadow, unsigned& outDimension );
