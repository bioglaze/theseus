#include "gameobject.h"

struct teGameObjectImpl
{
    unsigned components = 0;
    const char* name = nullptr;
};

constexpr unsigned MaxGameObjects = 1000;
teGameObjectImpl gameObjects[ MaxGameObjects ];
unsigned gameObjectCount = 0;

teGameObject teCreateGameObject( const char* name, unsigned components )
{
    teAssert( gameObjectCount + 1 < MaxGameObjects );

    teGameObject outGo;
    outGo.index = ++gameObjectCount;
    
    gameObjects[ outGo.index ].components = components;
    gameObjects[ outGo.index ].name = name;
    
    return outGo;
}

unsigned aeGameObjectGetComponents( unsigned index )
{
    teAssert( index < MaxGameObjects );

    return index < MaxGameObjects ? gameObjects[ index ].components : 0;
}
