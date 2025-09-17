#pragma once

void tePointLightSetParams( unsigned goIndex, const struct Vec3& position, float radius, const Vec3& color );
void tePointLightGetParams( unsigned goIndex, Vec3& outPosition, float& outRadius, Vec3& outColor );
float* tePointLightAccessRadius( unsigned goIndex );