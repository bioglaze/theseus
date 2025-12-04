#pragma once

void tePointLightSetParams( unsigned goIndex, float radius, const struct Vec3& color, float intensity );
void tePointLightGetParams( unsigned goIndex, Vec3& outPosition, float& outRadius, Vec3& outColor, float& outIntensity );
float* tePointLightAccessRadius( unsigned goIndex );
