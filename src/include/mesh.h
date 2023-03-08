#pragma once

struct teMesh
{
    unsigned index = 0;
};

teMesh teCreateCubeMesh();
unsigned teMeshGetSubMeshCount( const teMesh& mesh );
teMesh& teMeshRendererGetMesh( unsigned gameObjectIndex );
void teMeshRendererSetMesh( unsigned gameObjectIndex, teMesh* mesh );
void teMeshRendererSetMaterial( unsigned gameObjectIndex, const struct teMaterial& material, unsigned subMeshIndex );
const teMaterial& teMeshRendererGetMaterial( unsigned gameObjectIndex, unsigned subMeshIndex );
