#pragma once

struct teScene
{
    unsigned index = 0;
};

teScene teCreateScene();
void teSceneAdd( const teScene& scene, unsigned gameObjectIndex );
