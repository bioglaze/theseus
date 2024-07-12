#pragma once

struct teScene
{
    unsigned index = 0;
};

// \param directonalShadowMapDimension Shadow map dimension. 0 means that the light does not cast shadow.
teScene teCreateScene( unsigned directonalShadowMapDimension );
void teSceneAdd( const teScene& scene, unsigned gameObjectIndex );
void teSceneRender( const teScene& scene, const struct teShader* skyboxShader, const struct teTextureCube* skyboxTexture, const struct teMesh* skyboxMesh, const teShader& momentsShader, const struct Vec3& dirLightPosition );
bool teScenePointInsideAABB( const teScene& scene, const Vec3& point );
void teSceneSetupDirectionalLight( const teScene& scene, const Vec3& color, const Vec3& direction );
unsigned teSceneGetMaxGameObjects();
// \return 0 if the game object at index i doesn't exist.
unsigned teSceneGetGameObjectIndex( const teScene& scene, unsigned i );