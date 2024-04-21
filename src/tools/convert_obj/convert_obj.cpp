// Theseus engine OBJ converter.
// Author: Timo Wiren
// Modified: 2024-04-21
// Limitations:
//   - Only triangulated meshes currently work.
//   - Face indices are 16-bit.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "vec3.h"
#include "meshoptimizer.h"

struct UV
{
    float u, v;
};

struct Face
{
    unsigned posInd[ 3 ] = {};
	unsigned uvInd[ 3 ] = {};
	unsigned normInd[ 3 ] = {};
};

struct VertexInd
{
    unsigned short a, b, c;
};

struct Mesh
{
    Face* faces = nullptr;
    unsigned faceCount = 0;

    Vec3* finalPositions = nullptr;
    UV* finalUVs = nullptr;
    Vec3* finalNormals = nullptr;
    VertexInd* finalFaces = nullptr;
    unsigned int* meshletVertices = nullptr;
    unsigned int* meshletTriangles = nullptr;
    unsigned finalVertexCount = 0;
    unsigned finalFaceCount = 0;
    
    Vec3 aabbMin{ 99999, 99999, 99999 };
    Vec3 aabbMax{ -99999, -99999, -99999 };
};

Mesh* meshes = nullptr;
unsigned meshCount = 0;

Vec3* allPositions = nullptr;
Vec3* allNormals = nullptr;
UV* allUVs = nullptr;
unsigned totalPositionCount = 0;
unsigned totalUVCount = 0;
unsigned totalNormalCount = 0;

void WriteT3d( const char* path )
{
    FILE* file = fopen( path, "wb" );

    if (file == nullptr)
    {
        printf( "Could not open file for writing: %s\n", path );
        return;
    }

    const char buffer[] = { "t3d0000" };
    fwrite( buffer, sizeof( char ), sizeof( buffer ), file );
    fwrite( &meshCount, 1, 4, file );

    for (unsigned m = 0; m < meshCount; ++m)
    {
        fwrite( &meshes[ m ].aabbMin, 1, 3 * 4, file );
        fwrite( &meshes[ m ].aabbMax, 1, 3 * 4, file );
        fwrite( &meshes[ m ].finalFaceCount, 1, 4, file );
        fwrite( meshes[ m ].finalFaces, 3 * 2, meshes[ m ].finalFaceCount, file );

        // Pad to 4 bytes to prevent UB when reading.
        if (meshes[ m ].finalFaceCount % 2 != 0)
        {
            short dummy = 0;
            fwrite( &dummy, 1, 2, file );
        }
        
        fwrite( &meshes[ m ].finalVertexCount, 4, 1, file );
        fwrite( meshes[ m ].finalPositions, 3 * 4, meshes[ m ].finalVertexCount, file );
        fwrite( meshes[ m ].finalUVs, 2 * 4, meshes[ m ].finalVertexCount, file );
        fwrite( meshes[ m ].finalNormals, 3 * 4, meshes[ m ].finalVertexCount, file );
    }

    fclose( file );
}

bool AlmostEquals( const Vec3& v1, const Vec3& v2 )
{
    return fabs( v1.x - v2.x ) < 0.0001f &&
           fabs( v1.y - v2.y ) < 0.0001f &&
           fabs( v1.z - v2.z ) < 0.0001f;
}

bool AlmostEquals( const UV& uv1, const UV& uv2 )
{
    return fabs( uv1.u - uv2.u ) < 0.0001f && fabs( uv1.v - uv2.v ) < 0.0001f;
}

void BuildMeshlets( Mesh& mesh )
{
    const size_t maxVertices = 64;
    const size_t maxTriangles = 124;
    const float coneWeight = 0.0f;

    const size_t maxMeshlets = meshopt_buildMeshletsBound( mesh.finalFaceCount * 3, maxVertices, maxTriangles );
    meshopt_Meshlet* meshlets = new meshopt_Meshlet[ maxMeshlets ];
    mesh.meshletVertices = new unsigned int[ maxVertices * maxMeshlets ];
    mesh.meshletTriangles = new unsigned int[ maxTriangles * maxMeshlets * 3 ];

    const size_t meshletCount = meshopt_buildMeshlets( meshlets, mesh.meshletVertices, (unsigned char*)mesh.meshletTriangles, &mesh.finalFaces[ 0 ].a,
        mesh.finalFaceCount * 3, &mesh.finalPositions[ 0 ].x, mesh.finalVertexCount, sizeof(Vec3), maxVertices, maxTriangles, coneWeight );

}

void CreateFinalGeometry( Mesh& mesh )
{
    VertexInd newFace = {};

    // TODO: Don't hard-code these array sizes.
    mesh.finalPositions = new Vec3[ 153636 ];
    mesh.finalUVs = new UV[ 153636 ];
    mesh.finalNormals = new Vec3[ 153636 ];
    mesh.finalFaces = new VertexInd[ 128343 ];
    mesh.finalFaceCount = 0;
    
    for (unsigned f = 0; f < mesh.faceCount; ++f)
    {
        Vec3 pos = allPositions[ mesh.faces[ f ].posInd[ 0 ] ];
        Vec3 norm = allNormals[ mesh.faces[ f ].normInd[ 0 ] ];
        UV uv = allUVs[ mesh.faces[ f ].uvInd[ 0 ] ];

        bool found = false;

        for (unsigned i = 0; i < mesh.finalFaceCount; ++i)
        {
            if (AlmostEquals( pos, mesh.finalPositions[ mesh.finalFaces[ i ].a ] ) &&
                AlmostEquals( uv, mesh.finalUVs[ mesh.finalFaces[ i ].a ] ) &&
                AlmostEquals( norm, mesh.finalNormals[ mesh.finalFaces[ i ].a ] ))
            {
                found = true;
                newFace.a = mesh.finalFaces[ i ].a;
                break;
            }
        }

        if (!found)
        {
            mesh.finalPositions[ mesh.finalVertexCount ] = pos;
            mesh.finalUVs[ mesh.finalVertexCount ] = uv;
            mesh.finalNormals[ mesh.finalVertexCount ] = norm;

            ++mesh.finalVertexCount;
            assert( mesh.finalVertexCount < 153636 );
            
            newFace.a = (unsigned short)(mesh.finalVertexCount - 1);
        }

        pos = allPositions[ mesh.faces[ f ].posInd[ 1 ] ];
        norm = allNormals[ mesh.faces[ f ].normInd[ 1 ] ];
        uv = allUVs[ mesh.faces[ f ].uvInd[ 1 ] ];

        found = false;

        for (unsigned i = 0; i < mesh.finalFaceCount; ++i)
        {
            if (AlmostEquals( pos, mesh.finalPositions[ mesh.finalFaces[ i ].b ] ) &&
                AlmostEquals( uv, mesh.finalUVs[ mesh.finalFaces[ i ].b ] ) &&
                AlmostEquals( norm, mesh.finalNormals[ mesh.finalFaces[ i ].b ] ))
            {
                found = true;
                newFace.b = mesh.finalFaces[ i ].b;
                break;
            }
        }

        if (!found)
        {
            mesh.finalPositions[ mesh.finalVertexCount ] = pos;
            mesh.finalUVs[ mesh.finalVertexCount ] = uv;
            mesh.finalNormals[ mesh.finalVertexCount ] = norm;

            ++mesh.finalVertexCount;
            assert( mesh.finalVertexCount < 153636 );

            newFace.b = (unsigned short)(mesh.finalVertexCount - 1);
        }

        pos = allPositions[ mesh.faces[ f ].posInd[ 2 ] ];
        norm = allNormals[ mesh.faces[ f ].normInd[ 2 ] ];
        uv = allUVs[ mesh.faces[ f ].uvInd[ 2 ] ];

        found = false;

        for (unsigned i = 0; i < mesh.finalFaceCount; ++i)
        {
            if (AlmostEquals( pos, mesh.finalPositions[ mesh.finalFaces[ i ].c ] ) &&
                AlmostEquals( uv, mesh.finalUVs[ mesh.finalFaces[ i ].c ] ) &&
                AlmostEquals( norm, mesh.finalNormals[ mesh.finalFaces[ i ].c ] ))
            {
                found = true;
                newFace.c = mesh.finalFaces[ i ].c;
                break;
            }
        }

        if (!found)
        {
            mesh.finalPositions[ mesh.finalVertexCount ] = pos;
            mesh.finalUVs[ mesh.finalVertexCount ] = uv;
            mesh.finalNormals[ mesh.finalVertexCount ] = norm;

            ++mesh.finalVertexCount;
            assert( mesh.finalVertexCount < 153636 );

            newFace.c = (unsigned short)(mesh.finalVertexCount - 1);
        }

        mesh.finalFaces[ f ] = newFace;
        ++mesh.finalFaceCount;
        assert( mesh.finalFaceCount < 128343 );
    }

    for (unsigned i = 0; i < mesh.finalVertexCount; ++i)
    {
        if (mesh.finalPositions[ i ].x < mesh.aabbMin.x)
        {
            mesh.aabbMin.x = mesh.finalPositions[ i ].x;
        }

        if (mesh.finalPositions[ i ].y < mesh.aabbMin.y)
        {
            mesh.aabbMin.y = mesh.finalPositions[ i ].y;
        }

        if (mesh.finalPositions[ i ].z < mesh.aabbMin.z)
        {
            mesh.aabbMin.z = mesh.finalPositions[ i ].z;
        }

        if (mesh.finalPositions[ i ].x > mesh.aabbMax.x)
        {
            mesh.aabbMax.x = mesh.finalPositions[ i ].x;
        }
        
        if (mesh.finalPositions[ i ].y > mesh.aabbMax.y)
        {
            mesh.aabbMax.y = mesh.finalPositions[ i ].y;
        }
        
        if (mesh.finalPositions[ i ].z > mesh.aabbMax.z)
        {
            mesh.aabbMax.z = mesh.finalPositions[ i ].z;
        }
    }
}

void InitMeshArrays( FILE* file )
{
    char line[ 255 ];

    meshes = new Mesh[ 10000 ];
    
    unsigned faceCount = 0;
    
    while (fgets( line, 255, file ) != nullptr)
    {
        char input[ 255 ];
        sscanf( line, "%254s", input );
        
        if (strstr( input, "vn" ))
        {
            ++totalNormalCount;
        }
        else if (strstr( input, "vt" ))
        {
            ++totalUVCount;
        }
        else if (strchr( input, 'f' ))
        {
            Face face;
            Face face2;
            const int res = sscanf( line, "%254s %u/%u/%u %u/%u/%u %u/%u/%u %u/%u/%u", input, &face.posInd[ 0 ], &face.uvInd[ 0 ], &face.normInd[ 0 ],
                &face.posInd[ 1 ], &face.uvInd[ 1 ], &face.normInd[ 1 ], &face.posInd[ 2 ], &face.uvInd[ 2 ], &face.normInd[ 2 ],
                &face2.posInd[ 0 ], &face2.uvInd[ 0 ], &face2.normInd[ 0 ] );
            const bool isQuad = res == 13;
            faceCount += isQuad ? 2 : 1;
        }
        else if (strchr( input, 's' ) && strstr( input, "usemtl" ) == nullptr)
        {
            char str1[ 128 ];
            char smoothName[ 128 ];
            sscanf( line, "%127s %127s", str1, smoothName );
            
            if (strstr( smoothName, "off" ) == nullptr)
            {
                printf( "Warning: The file contains smoothing groups. They are not supported by the converter.\n" );
            }
        }
        else if (strchr( input, 'v' ))
        {
            ++totalPositionCount;
        }
        
        if (strcmp( input, "o" ) == 0 || strcmp( input, "g" ) == 0)
        {
            if (meshCount > 0)
            {
                meshes[ meshCount - 1 ].faceCount = faceCount;
                meshes[ meshCount - 1 ].faces = new Face[ faceCount ];
                faceCount = 0;
            }

            ++meshCount;
        }
    }

    meshes[ meshCount - 1 ].faceCount = faceCount;
    meshes[ meshCount - 1 ].faces = new Face[ faceCount ];

    allPositions = new Vec3[ totalPositionCount ];
    allNormals = new Vec3[ totalNormalCount ];
    allUVs = new UV[ totalUVCount ];
    
    fseek( file, 0, SEEK_SET );
    
    unsigned positionIndex = 0;
    unsigned uvIndex = 0;
    unsigned normalIndex = 0;
    
    while (fgets( line, 255, file ) != nullptr)
    {
        char input[ 255 ];
        sscanf( line, "%254s", input );
        
        if (strstr( input, "vn" ))
        {
            Vec3& normal = allNormals[ normalIndex ];
            sscanf( line, "%254s %f %f %f", input, &normal.x, &normal.y, &normal.z );
            ++normalIndex;
        }
        else if (strstr( input, "vt" ))
        {
            float u, v;
            sscanf( line, "%254s %f %f", input, &u, &v );
            v = 1 - v;
            allUVs[ uvIndex ].u = u;
            allUVs[ uvIndex ].v = v;
            ++uvIndex;
        }
        else if (strchr( input, 'v'))
        {
            Vec3& pos = allPositions[ positionIndex ];
            sscanf( line, "%254s %f %f %f", input, &pos.x, &pos.y, &pos.z );
            ++positionIndex;
        }
    }
}

int main( int argc, char* argv[] )
{
    if (argc != 2 || !strstr( argv[ 1 ], ".obj" ))
    {
        printf( "usage: ./convert_obj file.obj\n" );
        return 0;
    }
    
    FILE* file = fopen( argv[ 1 ], "rb" );
    
    if (!file)
    {
        printf( "Could not open %s\n", argv[ 1 ] );
        return 1;
    }
    
    InitMeshArrays( file );
    
    fseek( file, 0, SEEK_SET );
    
    char line[ 255 ];
    unsigned faceIndex = 0;
    meshCount = 0;
    
    while (fgets( line, 255, file ) != nullptr)
    {
		char input[ 255 ] = {};
        sscanf( line, "%254s", input );

        if (strcmp( input, "o" ) == 0 || strcmp( input, "g" ) == 0)
        {
            ++meshCount;
            faceIndex = 0;
        }
        else if (strchr( input, 'f' ))
        {
            Face& face = meshes[ meshCount - 1 ].faces[ faceIndex ];
            Face& face2 = meshes[ meshCount - 1 ].faces[ faceIndex + 1 ];
            const int err = sscanf( line, "%254s %u/%u/%u %u/%u/%u %u/%u/%u %u/%u/%u", input, &face.posInd[ 0 ], &face.uvInd[ 0 ], &face.normInd[ 0 ],
                                    &face.posInd[ 1 ], &face.uvInd[ 1 ], &face.normInd[ 1 ], &face.posInd[ 2 ], &face.uvInd[ 2 ], &face.normInd[ 2 ],
                                    &face2.posInd[ 1 ], &face2.uvInd[ 1 ], &face2.normInd[ 1 ] );
            bool isQuad = err == 13;
            
            if (isQuad)
            {
                printf( "fourth face: %d, %d, %d\n", face2.posInd[ 1 ], face2.uvInd[ 1 ], face2.normInd[ 1 ]);
                
                face2.posInd[ 0 ] = face.posInd[ 2 ];
                face2.uvInd[ 0 ] = face.uvInd[ 2 ];
                face2.normInd[ 0 ] = face.normInd[ 2 ];

                face2.posInd[ 2 ] = face.posInd[ 0 ];
                face2.uvInd[ 2 ] = face.uvInd[ 0 ];
                face2.normInd[ 2 ] = face.normInd[ 0 ];

                // .obj indexing starts at 1, so adjust for zero-based indexing.
                --face2.posInd[ 0 ];
                --face2.posInd[ 1 ];
                --face2.posInd[ 2 ];

                --face2.uvInd[ 0 ];
                --face2.uvInd[ 1 ];
                --face2.uvInd[ 2 ];

                --face2.normInd[ 0 ];
                --face2.normInd[ 1 ];
                --face2.normInd[ 2 ];
            }
            
            // .obj indexing starts at 1, so adjust for zero-based indexing.
            --face.posInd[ 0 ];
            --face.posInd[ 1 ];
            --face.posInd[ 2 ];

            --face.uvInd[ 0 ];
            --face.uvInd[ 1 ];
            --face.uvInd[ 2 ];

            --face.normInd[ 0 ];
            --face.normInd[ 1 ];
            --face.normInd[ 2 ];

            faceIndex += isQuad ? 2 : 1;
        }
    }
    
    fclose( file );
    
	char outPath[ 260 ] = {};
    strncpy( outPath, argv[ 1 ], 259 );
    char* extension = strstr( outPath, ".obj" );
    assert( extension );
    extension[ 1 ] = 't';
    extension[ 2 ] = '3';
    extension[ 3 ] = 'd';

    for (unsigned m = 0; m < meshCount; ++m)
    {
        CreateFinalGeometry( meshes[ m ] );
        BuildMeshlets( meshes[ m ] );
    }
    
    WriteT3d( outPath );
    return 0;
}

