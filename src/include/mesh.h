#pragma once

enum class teTopology { Triangles, Lines };

struct teMesh
{
    unsigned index = 0;
    teTopology topology = teTopology::Triangles;
};

teMesh teCreateCubeMesh();
teMesh teCreateQuadMesh();
teMesh teLoadMesh( const struct teFile& file );
unsigned teMeshGetSubMeshCount( const teMesh* mesh );
char* teMeshGetSubMeshName( const teMesh& mesh, unsigned subMeshIndex );
teMesh* teMeshRendererGetMesh( unsigned gameObjectIndex );
void teMeshRendererSetMesh( unsigned gameObjectIndex, teMesh* mesh );
void teMeshRendererSetMaterial( unsigned gameObjectIndex, const struct teMaterial& material, unsigned subMeshIndex );
void teMeshRendererSetEnabled( unsigned gameObjectIndex, bool enable );
bool teMeshRendererIsEnabled( unsigned gameObjectIndex );
const teMaterial& teMeshRendererGetMaterial( unsigned gameObjectIndex, unsigned subMeshIndex );
unsigned teMeshGetIndexCount( const teMesh& mesh, unsigned subMeshIndex );
unsigned teMeshGetUVCount( const teMesh& mesh, unsigned subMeshIndex );
void teMeshGetSubMeshLocalAABB( const teMesh& mesh, unsigned subMeshIndex, struct Vec3& outAABBMin, Vec3& outAABBMax );
