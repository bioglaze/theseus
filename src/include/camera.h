#pragma once

enum class teProjectionType { Perspective, Orthographic };
enum class teClearFlag { DepthAndColor, Depth, DontClear };

float teCameraGetFovDegrees( unsigned index );
float teCameraGetFar( unsigned index );
void teCameraSetProjection( unsigned index, float fovDegrees, float aspect, float nearDepth, float farDepth );
void teCameraSetProjection( unsigned index, float left, float right, float bottom, float top, float aNear, float aFar );
void teCameraGetProjection( unsigned index, float& outFovDegrees, float& outAspect, float& outNearDepth, float& outFarDepth );
void teCameraSetProjectionType( unsigned index, teProjectionType projectionType );
void teCameraSetClear( unsigned index, teClearFlag clearFlag, const struct Vec4& color );
void teCameraGetClear( unsigned index, teClearFlag& outClearFlag, Vec4& outColor );
struct Matrix& teCameraGetProjection( unsigned index );
struct teTexture2D& teCameraGetColorTexture( unsigned index );
teTexture2D& teCameraGetDepthTexture( unsigned index );
