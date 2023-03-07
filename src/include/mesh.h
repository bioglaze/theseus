#pragma once

struct teMesh
{
    unsigned index = 0;
};

teMesh teCreateCubeMesh();
unsigned teMeshGetSubMeshCount( const teMesh& mesh );
teMesh& teMeshRendererGetMesh( unsigned gameObjectIndex );
