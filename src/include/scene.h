#pragma once

struct teScene
{
    unsigned index = 0;
};

teScene teCreateScene();
void teSceneAdd( const teScene& scene, unsigned gameObjectIndex );
void teSceneRender( const teScene& scene, const struct teShader* skyboxShader, const struct teTextureCube* skyboxTexture, const struct teMesh* skyboxMesh );
bool teScenePointInsideAABB( const teScene& scene, const struct Vec3& point );