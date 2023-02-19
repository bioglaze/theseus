#pragma once

enum class teProjectionType { Perspective, Orthographic };
void teCameraSetProjection( unsigned index, float fovDegrees, float aspect, float nearDepth, float farDepth );
void teCameraSetProjection( unsigned index, float left, float right, float bottom, float top, float aNear, float aFar );
void teCameraGetProjection( unsigned index, float& outFovDegrees, float& outAspect, float& outNearDepth, float& outFarDepth );
void teCameraSetProjectionType( unsigned index, teProjectionType projectionType );
struct Matrix& teCameraGetProjection( unsigned index );
struct teTexture2D& teCameraGetColorTexture( unsigned index );
teTexture2D& teCameraGetColorResolveTexture( unsigned index );
teTexture2D& teCameraGetDepthResolveTexture( unsigned index );
teTexture2D& teCameraGetDepthTexture( unsigned index );
