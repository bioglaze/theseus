#include "mesh.h"
#include "material.h"
#include "vec3.h"

static constexpr unsigned MaxMeshes = 10000;
static constexpr unsigned MaxMaterials = 1000;

struct SubMesh
{
    unsigned indicesOffset = 0;
    unsigned indexCount = 0;
    unsigned uvOffset = 0;
    unsigned uvCount = 0;
    unsigned positionOffset = 0;
    unsigned positionCount = 0;

    Vec3 aabbMin;
    Vec3 aabbMax;
};

struct MeshImpl
{
    SubMesh* subMeshes = nullptr;
    unsigned subMeshCount = 0;
};

struct MeshRenderer
{
    teMesh* mesh = nullptr;
    teMaterial materials[ MaxMaterials ];
};

static MeshImpl meshes[ MaxMeshes ];
static unsigned meshIndex = 0;
static struct MeshRenderer meshRenderers[ MaxMeshes ];

teMesh teCreateCubeMesh()
{
    teAssert( meshIndex < MaxMeshes );

    teMesh outMesh;
    outMesh.index = ++meshIndex;

    const float positions[ 30 * 3 ] =
    {
        1, -1, -1,  1, -1, 1, -1, -1, 1, 1, 1, -1, -1, 1, -1, -1, 1, 1,
        1, -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1, 1, 1, 1, -1, -1, 1,
        -1, -1, 1, -1, 1, 1, -1, -1, -1, 1, 1, -1, 1, -1, -1, -1, -1, -1,
        -1, -1, -1, 1, -1, -1, 1, 1, 1, 1, 1, -1, 1, -1, 1, 1, -1, -1,
        1, 1, 1, -1, 1, 1, -1, 1, 1, -1, 1, -1, -1, 1, -1, 1, 1, -1
    };

    const float uvs[ 30 * 2 ] =
    {
        0, 1, 0, 2, -1, 2, 0, 0, -1, 0, -1, 1, 0, 1, 0, 0, -1, 0, 0, 1,
        0, 0, -1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 2, -1, 2, -1, 1, 0, 1,
        0, 1, 0, 0, -1, 1, 0, 1, 0, 0, -1, 0, 0, 0, 1, 0, -1, 1, 0, 1
    };

    const unsigned short indices[ 12 * 3 ] =
    {
        0, 1, 2, 18, 19, 2, 3, 4, 5, 20, 21, 5, 6, 7, 8, 22, 23, 8,
        9, 10, 11, 24, 25, 11, 12, 13, 14, 26, 27, 14, 15, 16, 17, 28, 29, 17
    };

    meshes[ outMesh.index ].subMeshCount = 1;
    meshes[ outMesh.index ].subMeshes = new SubMesh[ 1 ]();
    //meshes[ outMesh.index ].subMeshes[ 0 ].positionOffset = AddPositions( positions, sizeof( positions ) );
    //meshes[ outMesh.index ].subMeshes[ 0 ].positionCount = 30;
    //meshes[ outMesh.index ].subMeshes[ 0 ].uvOffset = AddUVs( uvs, sizeof( uvs ) );
    //meshes[ outMesh.index ].subMeshes[ 0 ].uvCount = 30;
    //meshes[ outMesh.index ].subMeshes[ 0 ].indicesOffset = AddIndices( indices, sizeof( indices ) );
    //meshes[ outMesh.index ].subMeshes[ 0 ].indexCount = 12;
    meshes[ outMesh.index ].subMeshes[ 0 ].aabbMin = Vec3( -1, -1, -1 );
    meshes[ outMesh.index ].subMeshes[ 0 ].aabbMax = Vec3( 1, 1, 1 );

    return outMesh;
}

unsigned teMeshGetSubMeshCount( const teMesh& mesh )
{
    return meshes[ mesh.index ].subMeshCount;
}

teMesh& teMeshRendererGetMesh( unsigned gameObjectIndex )
{
    teAssert( gameObjectIndex < MaxMeshes );
    return *meshRenderers[ gameObjectIndex ].mesh;
}

void teMeshRendererSetMesh( unsigned gameObjectIndex, teMesh* mesh )
{
    teAssert( gameObjectIndex < MaxMeshes );
    meshRenderers[ gameObjectIndex ].mesh = mesh;
}

const teMaterial& teMeshRendererGetMaterial( unsigned gameObjectIndex, unsigned subMeshIndex )
{
    teAssert( subMeshIndex < MaxMaterials );

    return meshRenderers[ gameObjectIndex ].materials[ subMeshIndex ];
}

void teMeshRendererSetMaterial( unsigned gameObjectIndex, const struct teMaterial& material, unsigned subMeshIndex )
{
    teAssert( gameObjectIndex < MaxMeshes );
    teAssert( subMeshIndex < MaxMaterials );

    meshRenderers[ gameObjectIndex ].materials[ subMeshIndex ] = material;
}
