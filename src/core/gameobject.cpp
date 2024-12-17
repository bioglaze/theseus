#include "gameobject.h"
#include "te_stdlib.h"

struct teGameObjectImpl
{
    unsigned components = 0;
    const char* name = "unnamed";
};

constexpr unsigned MaxGameObjects = 10000;
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

unsigned teGameObjectGetComponents( unsigned index )
{
    teAssert( index < MaxGameObjects );

    return index < MaxGameObjects ? gameObjects[ index ].components : 0;
}

const char* teGameObjectGetName( unsigned index )
{
    return gameObjects[ index ].name;
}

void teGameObjectAddComponent( unsigned index, teComponent component )
{
    teAssert( index < MaxGameObjects );

    gameObjects[ index ].components |= component;
}
