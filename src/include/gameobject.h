#pragma once

enum teComponent : unsigned
{
    Transform = 1,
    Camera = 2,
    MeshRenderer = 4,
    DirectionalLight = 8,
    SpotLight = 16,
    PointLight = 32,
};

struct teGameObject
{
    unsigned index = 0;
};

unsigned teGameObjectGetComponents( unsigned index );
teGameObject teCreateGameObject( const char* name, unsigned components );
