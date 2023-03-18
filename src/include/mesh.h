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
unsigned teMeshGetPositionOffset( unsigned index, unsigned subMeshIndex );
unsigned teMeshGetIndexOffset( unsigned index, unsigned subMeshIndex );
unsigned teMeshGetIndexCount( unsigned index, unsigned subMeshIndex );
unsigned teMeshGetUVOffset( unsigned index, unsigned subMeshIndex );
unsigned teMeshGetUVCount( unsigned index, unsigned subMeshIndex );

