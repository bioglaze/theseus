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
teTexture2D& teCameraGetDepthNormalsTexture( unsigned index );
/// \param worldPoint Point in the world.
/// \param viewWidth Camera's viewport width.
/// \param viewHeight Camera's viewport height.
/// \param outScreenPoint Screen point for worldPoint.
/// \return true if the point lies in front of the camera. False if it lies behind the camera
bool teCameraGetScreenPoint( unsigned index, const struct Vec3& worldPoint, float viewWidth, float viewHeight, Vec3& outScreenPoint );
