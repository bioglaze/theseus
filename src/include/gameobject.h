#pragma once

enum teComponent : unsigned
{
    Transform = 1,
    Camera = 2,
    MeshRenderer = 4,
    PointLight = 8,
    SpotLight = 16
};

struct teGameObject
{
    unsigned index = 0;
};

unsigned teGameObjectGetComponents( unsigned index );
teGameObject teCreateGameObject( const char* name, unsigned components );
void teGameObjectSetName( unsigned index, const char* name );
const char* teGameObjectGetName( unsigned index );
void teGameObjectAddComponent( unsigned index, teComponent component );
