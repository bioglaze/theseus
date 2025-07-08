#pragma once

enum teComponent : unsigned
{
    Transform = 1,
    Camera = 2,
    MeshRenderer = 4,
    Light = 8,
};

struct teGameObject
{
    unsigned index = 0;
};

unsigned teGameObjectGetComponents( unsigned index );
teGameObject teCreateGameObject( const char* name, unsigned components );
const char* teGameObjectGetName( unsigned index );
void teGameObjectAddComponent( unsigned index, teComponent component );
