#pragma once

void tePointLightSetParams( unsigned goIndex, float radius, const struct Vec3& color );
void tePointLightGetParams( unsigned goIndex, Vec3& outPosition, float& outRadius, Vec3& outColor );
float* tePointLightAccessRadius( unsigned goIndex );
