#include "mesh.h"
#include "material.h"
#include "file.h"
#include "te_stdlib.h"
#include "vec3.h"
#include <stdio.h>

unsigned AddPositions( const float* positions, unsigned bytes );
unsigned AddNormals( const float* normals, unsigned bytes );
unsigned AddTangents( const float* tangents, unsigned bytes );
unsigned AddIndices( const unsigned short* indices, unsigned bytes );
unsigned AddUVs( const float* uvs, unsigned bytes );

static constexpr unsigned MaxMeshes = 10000;
static constexpr unsigned MaxMaterials = 1000;

// Copied from meshoptimizer.
struct meshopt_Meshlet
{
    /* offsets within meshlet_vertices and meshlet_triangles arrays with meshlet data */
    unsigned int vertex_offset;
    unsigned int triangle_offset;

    /* number of vertices and triangles used in the meshlet; data is stored in consecutive range defined by offset and count */
    unsigned int vertex_count;
    unsigned int triangle_count;
};

struct SubMesh
{
    unsigned indicesOffset = 0;
    unsigned indexCount = 0;
    unsigned uvOffset = 0;
    unsigned uvCount = 0;
    unsigned positionOffset = 0;
    unsigned positionCount = 0;
    unsigned normalOffset = 0;
    unsigned normalCount = 0;
    unsigned tangentOffset = 0;
    unsigned tangentCount = 0;
    unsigned meshletCount = 0;

    Vec3     aabbMin;
    Vec3     aabbMax;

    unsigned nameIndex = 0; // Index into MeshImpl::names.
    meshopt_Meshlet* meshlets = nullptr;
};

struct MeshImpl
{
    SubMesh* subMeshes = nullptr;
    unsigned subMeshCount = 0;
    char*    names = nullptr;
};

struct MeshRenderer
{
    teMesh*    mesh = nullptr;
    teMaterial materials[ MaxMaterials ];
    bool       isSubMeshCulled[ MaxMaterials ];
    bool       enabled = true;
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

    const float tangents[ 30 * 4 ] = // FIXME: these values are not correct.
    {
        1, -1, -1,  1, -1, 1, -1, -1, 1, 1, 1, -1, -1, 1, -1, -1, 1, 1,
        1, -1, -1, 1, 1, -1, 1, 1, 1, 1, -1, 1, 1, 1, 1, -1, -1, 1,
        -1, -1, 1, -1, 1, 1, -1, -1, -1, 1, 1, -1, 1, -1, -1, -1, -1, -1,
        -1, -1, -1, 1, -1, -1, 1, 1, 1, 1, 1, -1, 1, -1, 1, 1, -1, -1,
        1, 1, 1, -1, 1, 1, -1, 1, 1, -1, 1, -1, -1, 1, -1, 1, 1, -1,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0
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
    meshes[ outMesh.index ].subMeshes[ 0 ].positionOffset = AddPositions( positions, sizeof( positions ) );
    meshes[ outMesh.index ].subMeshes[ 0 ].positionCount = 30;
    meshes[ outMesh.index ].subMeshes[ 0 ].uvOffset = AddUVs( uvs, sizeof( uvs ) );
    meshes[ outMesh.index ].subMeshes[ 0 ].uvCount = 30;
    meshes[ outMesh.index ].subMeshes[ 0 ].indicesOffset = AddIndices( indices, sizeof( indices ) );
    meshes[ outMesh.index ].subMeshes[ 0 ].indexCount = 12;
    meshes[ outMesh.index ].subMeshes[ 0 ].aabbMin = Vec3( -1, -1, -1 );
    meshes[ outMesh.index ].subMeshes[ 0 ].aabbMax = Vec3( 1, 1, 1 );
    meshes[ outMesh.index ].subMeshes[ 0 ].normalOffset = AddNormals( positions, sizeof( positions ) ); // TODO: normals, not positions.
    meshes[ outMesh.index ].subMeshes[ 0 ].normalCount = 30;
    meshes[ outMesh.index ].subMeshes[ 0 ].tangentOffset = AddTangents( tangents, sizeof( tangents ) );
    meshes[ outMesh.index ].subMeshes[ 0 ].tangentCount = 30;
    meshes[ outMesh.index ].subMeshes[ 0 ].nameIndex = 0;
    meshes[ outMesh.index ].names = (char*)teMalloc( 10 );
    meshes[ outMesh.index ].names[ 0 ] = 0;

    return outMesh;
}

teMesh teCreateQuadMesh()
{
    teAssert( meshIndex < MaxMeshes );

    teMesh outMesh;
    outMesh.index = ++meshIndex;

    const float positions[] =
    {
        0, 0, 0,
        1, 0, 0,
        1, 1, 0,
        0, 1, 0
    };

    const float tangents[] =
    {
        0, 0, 0, 0,
        1, 0, 0, 0,
        1, 1, 0, 0,
        0, 1, 0, 0
    };

    const float uvs[] =
    {
        0, 0,
        1, 0,
        1, 1,
        0, 1
    };

    const unsigned short indices[] =
    {
        0, 1, 2,
        2, 3, 0
    };

    meshes[ outMesh.index ].subMeshCount = 1;
    meshes[ outMesh.index ].subMeshes = new SubMesh[ 1 ]();
    meshes[ outMesh.index ].subMeshes[ 0 ].positionOffset = AddPositions( positions, sizeof( positions ) );
    meshes[ outMesh.index ].subMeshes[ 0 ].positionCount = 4;
    meshes[ outMesh.index ].subMeshes[ 0 ].uvOffset = AddUVs( uvs, sizeof( uvs ) );
    meshes[ outMesh.index ].subMeshes[ 0 ].uvCount = 4;
    meshes[ outMesh.index ].subMeshes[ 0 ].indicesOffset = AddIndices( indices, sizeof( indices ) );
    meshes[ outMesh.index ].subMeshes[ 0 ].indexCount = 2;
    meshes[ outMesh.index ].subMeshes[ 0 ].aabbMin = Vec3( -1, -1, -1 );
    meshes[ outMesh.index ].subMeshes[ 0 ].aabbMax = Vec3( 1, 1, 1 );
    meshes[ outMesh.index ].subMeshes[ 0 ].normalOffset = AddNormals( positions, sizeof( positions ) );
    meshes[ outMesh.index ].subMeshes[ 0 ].normalCount = 4;
    meshes[ outMesh.index ].subMeshes[ 0 ].tangentOffset = AddTangents( tangents, sizeof( tangents ) );
    meshes[ outMesh.index ].subMeshes[ 0 ].tangentCount = 4;
    meshes[ outMesh.index ].names = (char*)teMalloc( 10 );
    meshes[ outMesh.index ].names[ 0 ] = 0;

    return outMesh;
}

teMesh teLoadMesh( const teFile& file )
{
    teAssert( meshIndex < MaxMeshes );

    if (!file.data)
    {
        return teCreateCubeMesh();
    }

    teMesh outMesh;
    outMesh.index = ++meshIndex;

    // Header is something like "t3d0002" where the last numbers are version that is incremented when reading compatibility breaks.
    if (file.data[ 0 ] != 't' || file.data[ 1 ] != '3' || file.data[ 2 ] != 'd' || file.data[ 6 ] != '2')
    {
        printf( "%s has wrong version!\n", file.path );
        return outMesh;
    }

    unsigned char* pointer = &file.data[ 8 ];
    meshes[ outMesh.index ].subMeshCount = *((unsigned*)pointer);
    meshes[ outMesh.index ].subMeshes = new SubMesh[ meshes[ outMesh.index ].subMeshCount ]();
    pointer += 4;

    for (unsigned m = 0; m < meshes[ outMesh.index ].subMeshCount; ++m)
    {
        meshes[ outMesh.index ].subMeshes[ m ].aabbMin.x = *((float*)pointer);
        pointer += 4;
        meshes[ outMesh.index ].subMeshes[ m ].aabbMin.y = *((float*)pointer);
        pointer += 4;
        meshes[ outMesh.index ].subMeshes[ m ].aabbMin.z = *((float*)pointer);

        pointer += 4;
        meshes[ outMesh.index ].subMeshes[ m ].aabbMax.x = *((float*)pointer);
        pointer += 4;
        meshes[ outMesh.index ].subMeshes[ m ].aabbMax.y = *((float*)pointer);
        pointer += 4;
        meshes[ outMesh.index ].subMeshes[ m ].aabbMax.z = *((float*)pointer);

        pointer += 4;
        const unsigned faceCount = *((unsigned*)pointer);
        pointer += 4;
        meshes[ outMesh.index ].subMeshes[ m ].indicesOffset = AddIndices( (const unsigned short*)pointer, faceCount * 2 * 3 );
        meshes[ outMesh.index ].subMeshes[ m ].indexCount = faceCount;
        pointer += faceCount * 2 * 3;

        if (faceCount % 2 != 0)
        {
            pointer += 2;
        }

        const unsigned vertexCount = *((unsigned*)pointer);
        pointer += 4;
        meshes[ outMesh.index ].subMeshes[ m ].positionOffset = AddPositions( (float*)pointer, vertexCount * 3 * 4 );
        meshes[ outMesh.index ].subMeshes[ m ].positionCount = vertexCount;
        pointer += vertexCount * 3 * 4;
        meshes[ outMesh.index ].subMeshes[ m ].uvOffset = AddUVs( (float*)pointer, vertexCount * 2 * 4 );
        meshes[ outMesh.index ].subMeshes[ m ].uvCount = vertexCount;
        pointer += vertexCount * 2 * 4;
        meshes[ outMesh.index ].subMeshes[ m ].normalOffset = AddNormals( (float*)pointer, vertexCount * 3 * 4 );
        meshes[ outMesh.index ].subMeshes[ m ].normalCount = vertexCount;
        pointer += vertexCount * 3 * 4;
        meshes[ outMesh.index ].subMeshes[ m ].tangentOffset = AddTangents( (float*)pointer, vertexCount * 4 * 4 );
        meshes[ outMesh.index ].subMeshes[ m ].tangentCount = vertexCount;
        pointer += vertexCount * 4 * 4;
        meshes[ outMesh.index ].subMeshes[ m ].meshletCount = *((unsigned*)pointer);
        pointer += 4;
        meshes[ outMesh.index ].subMeshes[ m ].meshlets = (meshopt_Meshlet*)malloc( meshes[ outMesh.index ].subMeshes[ m ].meshletCount * sizeof( meshopt_Meshlet ) );
        memcpy( meshes[ outMesh.index ].subMeshes[ m ].meshlets, pointer, meshes[ outMesh.index ].subMeshes[ m ].meshletCount );
        pointer += meshes[ outMesh.index ].subMeshes[ m ].meshletCount * sizeof( meshopt_Meshlet );
        meshes[ outMesh.index ].subMeshes[ m ].nameIndex = *((unsigned*)pointer);
        pointer += 4;
    }

    unsigned namesSize = *((unsigned*)pointer);
    pointer += 4;
    meshes[ outMesh.index ].names = (char*)malloc( namesSize );
    memcpy( meshes[ outMesh.index ].names, pointer, namesSize );

    return outMesh;
}

void teMeshGetSubMeshLocalAABB( const teMesh& mesh, unsigned subMeshIndex, Vec3& outAABBMin, Vec3& outAABBMax )
{
    teAssert( subMeshIndex < MaxMaterials );

    outAABBMin = meshes[ mesh.index ].subMeshes[ subMeshIndex ].aabbMin;
    outAABBMax = meshes[ mesh.index ].subMeshes[ subMeshIndex ].aabbMax;
}

unsigned teMeshGetSubMeshCount( const teMesh& mesh )
{
    teAssert( mesh.index != 0);
    teAssert( mesh.index < MaxMeshes );
    return meshes[ mesh.index ].subMeshCount;
}

char* teMeshGetSubMeshName( const teMesh& mesh, unsigned subMeshIndex )
{
    return &meshes[ mesh.index ].names[ meshes[ mesh.index ].subMeshes[ subMeshIndex ].nameIndex ];
}

teMesh& teMeshRendererGetMesh( unsigned gameObjectIndex )
{
    teAssert( gameObjectIndex < MaxMeshes );
    return *meshRenderers[ gameObjectIndex ].mesh;
}

bool MeshRendererIsCulled( unsigned gameObjectIndex, unsigned subMeshIndex )
{
    teAssert( subMeshIndex < MaxMaterials );

    return meshRenderers[ gameObjectIndex ].isSubMeshCulled[ subMeshIndex ];
}

void MeshRendererSetCulled( unsigned gameObjectIndex, unsigned subMeshIndex, bool isCulled )
{
    teAssert( subMeshIndex < MaxMaterials );

    meshRenderers[ gameObjectIndex ].isSubMeshCulled[ subMeshIndex ] = isCulled;
}

void teMeshRendererSetMesh( unsigned gameObjectIndex, teMesh* mesh )
{
    teAssert( gameObjectIndex < MaxMeshes );
    meshRenderers[ gameObjectIndex ].mesh = mesh;
}

void teMeshRendererSetEnabled( unsigned gameObjectIndex, bool enable )
{
    teAssert( gameObjectIndex < MaxMeshes );
    meshRenderers[ gameObjectIndex ].enabled = enable;
}

bool teMeshRendererIsEnabled( unsigned gameObjectIndex )
{
    teAssert( gameObjectIndex < MaxMeshes );
    return meshRenderers[ gameObjectIndex ].enabled;
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

unsigned teMeshGetPositionOffset( const teMesh& mesh, unsigned subMeshIndex )
{
    teAssert( mesh.index != 0 );
    teAssert( subMeshIndex < meshes[ mesh.index ].subMeshCount );
    return meshes[ mesh.index ].subMeshes[ subMeshIndex ].positionOffset;
}

unsigned teMeshGetNormalOffset( const teMesh& mesh, unsigned subMeshIndex )
{
    teAssert( mesh.index != 0 );
    teAssert( subMeshIndex < meshes[ mesh.index ].subMeshCount );
    return meshes[ mesh.index ].subMeshes[ subMeshIndex ].normalOffset;
}

unsigned teMeshGetNormalCount( const teMesh& mesh, unsigned subMeshIndex )
{
    teAssert( mesh.index != 0 );
    teAssert( subMeshIndex < meshes[ mesh.index ].subMeshCount );
    return meshes[ mesh.index ].subMeshes[ subMeshIndex ].normalCount;
}

unsigned teMeshGetIndexOffset( const teMesh& mesh, unsigned subMeshIndex )
{
    teAssert( mesh.index != 0 );
    teAssert( subMeshIndex < meshes[ mesh.index ].subMeshCount );
    return meshes[ mesh.index ].subMeshes[ subMeshIndex ].indicesOffset;
}

unsigned teMeshGetIndexCount( const teMesh& mesh, unsigned subMeshIndex )
{
    teAssert( mesh.index != 0 );
    teAssert( subMeshIndex < meshes[ mesh.index ].subMeshCount );
    return meshes[ mesh.index ].subMeshes[ subMeshIndex ].indexCount;
}

unsigned teMeshGetUVOffset( const teMesh& mesh, unsigned subMeshIndex )
{
    teAssert( mesh.index != 0 );
    teAssert( subMeshIndex < meshes[ mesh.index ].subMeshCount );
    return meshes[ mesh.index ].subMeshes[ subMeshIndex ].uvOffset;
}

unsigned teMeshGetUVCount( const teMesh& mesh, unsigned subMeshIndex )
{
    teAssert( mesh.index != 0 );
    teAssert( subMeshIndex < meshes[ mesh.index ].subMeshCount );
    return meshes[ mesh.index ].subMeshes[ subMeshIndex ].uvCount;
}
