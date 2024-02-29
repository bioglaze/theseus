#pragma once

struct teMesh
{
    unsigned index = 0;
};

teMesh teCreateCubeMesh();
teMesh teCreateQuadMesh();
teMesh teLoadMesh( const struct teFile& file );
unsigned teMeshGetSubMeshCount( const teMesh& mesh );
teMesh& teMeshRendererGetMesh( unsigned gameObjectIndex );
void teMeshRendererSetMesh( unsigned gameObjectIndex, teMesh* mesh );
void teMeshRendererSetMaterial( unsigned gameObjectIndex, const struct teMaterial& material, unsigned subMeshIndex );
const teMaterial& teMeshRendererGetMaterial( unsigned gameObjectIndex, unsigned subMeshIndex );
unsigned teMeshGetPositionOffset( const teMesh& mesh, unsigned subMeshIndex );
unsigned teMeshGetIndexOffset( const teMesh& mesh, unsigned subMeshIndex );
unsigned teMeshGetIndexCount( const teMesh& mesh, unsigned subMeshIndex );
unsigned teMeshGetUVOffset( const teMesh& mesh, unsigned subMeshIndex );
unsigned teMeshGetUVCount( const teMesh& mesh, unsigned subMeshIndex );
void teMeshGetSubMeshLocalAABB( const teMesh& mesh, unsigned subMeshIndex, struct Vec3& outAABBMin, Vec3& outAABBMax );
