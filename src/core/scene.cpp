#include "scene.h"
#include "camera.h"
#include "frustum.h"
#include "gameobject.h"
#include "matrix.h"
#include "quaternion.h"
#include "renderer.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"

constexpr unsigned MAX_GAMEOBJECTS = 10000;

struct SceneImpl
{
    unsigned gameObjects[ MAX_GAMEOBJECTS ] = {};
};

SceneImpl scenes[ 2 ];
unsigned sceneIndex = 0;

teScene teCreateScene()
{
    teAssert( sceneIndex < 2 );

    teScene outScene;
    outScene.index = sceneIndex++;

    for (unsigned i = 0; i < MAX_GAMEOBJECTS; ++i)
    {
        scenes[ outScene.index ].gameObjects[ i ] = 0;
    }

    return outScene;
}

void teSceneAdd( const teScene& scene, unsigned gameObjectIndex )
{
    teAssert( scene.index < 2 );

    if ((teGameObjectGetComponents( gameObjectIndex ) & teComponent::Camera) != 0)
    {
        teAssert( teCameraGetColorTexture( gameObjectIndex ).index != -1 ); // Camera must have a render texture!
    }

    for (unsigned i = 0; i < MAX_GAMEOBJECTS; ++i)
    {
        if (scenes[ scene.index ].gameObjects[ i ] == gameObjectIndex)
        {
            return;
        }
    }

    for (unsigned i = 0; i < MAX_GAMEOBJECTS; ++i)
    {
        if (scenes[ scene.index ].gameObjects[ i ] == 0)
        {
            scenes[ scene.index ].gameObjects[ i ] = gameObjectIndex;
            return;
        }
    }

    teAssert( !"Too many game objects!" );
}
