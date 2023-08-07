#pragma once

struct teScene
{
    unsigned index = 0;
};

// \param directonalShadowMapDimension Shadow map dimension. 0 means that the light does not cast shadow.
teScene teCreateScene( unsigned directonalShadowMapDimension );
void teSceneAdd( const teScene& scene, unsigned gameObjectIndex );
void teSceneRender( const teScene& scene, const struct teShader* skyboxShader, const struct teTextureCube* skyboxTexture, const struct teMesh* skyboxMesh );
bool teScenePointInsideAABB( const teScene& scene, const struct Vec3& point );
void teSceneSetupDirectionalLight( const teScene& scene, const Vec3& color, const Vec3& direction );