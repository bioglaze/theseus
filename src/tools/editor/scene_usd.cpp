#include "file.h"
#include "gameobject.h"
#include "material.h"
#include "mesh.h"
#include "light.h"
#include "quaternion.h"
#include "scene.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

unsigned GetTranslateGizmoGoIndex();
teMaterial& GetMaterial( const char* name );

struct LoadedMesh
{
    teFile file;
    teMesh mesh;
};

constexpr unsigned MaxLoadedMeshes = 100;
LoadedMesh gLoadedMeshes[ MaxLoadedMeshes ];
unsigned gLoadedMeshCount = 0;

void ReadSceneArraySizes( FILE* file, unsigned& outGoCount, unsigned& outTextureCount, 
                          unsigned& outMaterialCount, unsigned& outMeshCount )
{
    outGoCount = 0;
    outTextureCount = 0;
    outMaterialCount = 0;
    outMeshCount = 0;

    char line[ 512 ];
    
    while (fgets( line, sizeof( line ), file ))
    {
        char input[ 255 ];
        sscanf( line, "%254s", input );
        //printf("input: %s\n", input);
        
        if (strstr( line, "def Xform" ))
        {
            printf("ReadArraySizes: found Xform\n");
            ++outGoCount;
        }
        else if (strstr( line, "def SphereLight" ))
        {
            printf("ReadArraySizes: found SphereLight\n");
        }
        else if (strstr( line, "#usda 1.0" ))
        {
            printf("ReadArraySizes: found usda\n");
        }
        else if (strstr( line, "string mesh" ))
        {
            ++outMeshCount;
        }
    }

    // Prevent malloc'ing 0 bytes
    if (outMeshCount == 0)
    {
        outMeshCount = 1;
    }
    if (outMaterialCount == 0)
    {
        outMaterialCount = 1;
    }
    if (outTextureCount == 0)
    {
        outTextureCount = 1;
    }
}

void LoadUsdScene( teScene& scene, const char* path )
{
    FILE* file = fopen( path, "rb" );
    if (!file)
    {
        printf( "Unable to open file %s for reading!\n", path );
        return;
    }
    
    unsigned sceneGoCount = 0;
    unsigned sceneTextureCount = 0;
    unsigned sceneMaterialCount = 0;
    unsigned sceneMeshCount = 0;
    ReadSceneArraySizes( file, sceneGoCount, sceneTextureCount, sceneMaterialCount, sceneMeshCount );
    teGameObject* sceneGos = (teGameObject*)malloc( sceneGoCount * sizeof( teGameObject ) );
    teTexture2D* sceneTextures = (teTexture2D*)malloc( sceneTextureCount * sizeof( teTexture2D ) );
    teMaterial* sceneMaterials = (teMaterial*)malloc( sceneMaterialCount * sizeof( teMaterial ) );
    teMesh* sceneMeshes = (teMesh*)malloc( sceneMeshCount * sizeof( teMesh ) );
    printf( "gos: %u, textures: %u, materials: %u, meshes: %u\n", sceneGoCount, sceneTextureCount, sceneMaterialCount, sceneMeshCount );

    fseek( file, 0, 0 );

    char line[ 512 ];

    unsigned goIndex = 0;
    unsigned meshIndex = 0;

    while (fgets( line, sizeof( line ), file ))
    {
        //printf("read line: %s\n", line );
        
        char input[ 255 ];
        sscanf( line, "%254s", input );

        if (strstr( line, "def Xform" ))
        {
            sceneGos[ goIndex ] = teCreateGameObject( "gameobject", teComponent::Transform );
            ++goIndex;
        }
        else if (strstr( line, "def SphereLight" ))
        {
            teGameObjectAddComponent( sceneGos[ goIndex - 1 ].index, teComponent::PointLight );
            tePointLightSetParams( sceneGos[ goIndex - 1 ].index, 2, Vec3( 1, 1, 1 ), 1.0f );
        }
        else if (strstr( line, "color3f" ))
        {
            Vec3 color = Vec3( 1, 1, 1 );
            char skip1[ 255 ] = {};
            char skip2[ 255 ] = {};
            char skip3[ 255 ] = {};
            sscanf( line, "%s %s %s (%f, %f, %f)", skip1, skip2, skip3, &color.x, &color.y, &color.z );

            tePointLightSetParams( sceneGos[ goIndex - 1 ].index, 2, color, 1.0f );
        }
        else if (strstr( line, "#usda 1.0" ))
        {
            printf("found usda\n");
        }
        else if (strstr( line, "string name" )) // for example: string name = "gameObject"
        {
            char a[ 256 ] = {};
            char b[ 256 ] = {};
            char c[ 256 ] = {};
            char name[ 256 ] = {};
            sscanf( line, "%s %s %s \"%s", a, b, c, name );
            size_t len = strlen( name );
            name[ len - 1 ] = 0;
            teGameObjectSetName( sceneGos[ goIndex - 1 ].index, name );
        }
        else if (strstr( line, "string mesh" ))
        {
            char a[ 256 ] = {};
            char b[ 256 ] = {};
            char c[ 256 ] = {};
            char meshPath[ 256 ] = {};
            sscanf( line, "%s %s %s \"%s", a, b, c, meshPath );
            size_t len = strlen( meshPath );
            meshPath[ len - 1 ] = 0;
            teGameObjectAddComponent( sceneGos[ goIndex - 1 ].index, teComponent::MeshRenderer );

            bool alreadyLoaded = false;
            for (unsigned i = 0; i < gLoadedMeshCount; ++i)
            {
                if (strcmp( gLoadedMeshes[ i ].file.path, meshPath ) == 0)
                {
                    alreadyLoaded = true;
                    teMeshRendererSetMesh( sceneGos[ goIndex - 1 ].index, &gLoadedMeshes[ i ].mesh );
                }
            }

            if (!alreadyLoaded)
            {
                teFile meshFile = teLoadFile( meshPath );

                if (meshFile.data)
                {
                    teMesh* mesh = &sceneMeshes[ meshIndex ];
                    ++meshIndex;
                    *mesh = teLoadMesh( meshFile );
                    gLoadedMeshes[ gLoadedMeshCount ].mesh = *mesh;
                    strcpy( gLoadedMeshes[ gLoadedMeshCount ].file.path, meshPath );
                    teMeshRendererSetMesh( sceneGos[ goIndex - 1 ].index, mesh );
                    ++gLoadedMeshCount;
                    if (gLoadedMeshCount >= MaxLoadedMeshes)
                    {
                        printf( "USD file contains more unique meshes than the loader can handle!\n" );
                        assert( false );
                    }
                }
            }
        }
        else if (strstr( line, "string material" ))
        {
            char a[ 256 ] = {};
            char b[ 256 ] = {};
            char c[ 256 ] = {};
            char name[ 256 ] = {};
            sscanf( line, "%s %s %s \"%s", a, b, c, name );
            const char* matEnd = strstr( line, "material" ) + strlen( "material" );
            int matIndex = 0;
            sscanf( matEnd, "%d", &matIndex );
            size_t len = strlen( name );
            name[ len - 1 ] = 0;
            teMeshRendererSetMaterial( sceneGos[ goIndex - 1 ].index, GetMaterial( name ), matIndex );
        }
        else if (strstr( line, "xformOp:translate" ))
        {
            Vec3 pos;
            char skip1[ 255 ] = {};
            char skip2[ 255 ] = {};
            char skip3[ 255 ] = {};
            sscanf( line, "%s %s %s (%f, %f, %f)", skip1, skip2, skip3, &pos.x, &pos.y, &pos.z );
            teTransformSetLocalPosition( sceneGos[ goIndex - 1 ].index, pos );
        }
    }

    fclose( file );
    
    for (unsigned i = 0; i < sceneGoCount; ++i)
    {
        teSceneAdd( scene, sceneGos[ i ].index );
    }
}

void SaveUsdScene( const teScene& scene, const char* path )
{
    FILE* outFile = fopen( path, "wb" );
    if (!outFile)
    {
        printf( "Unable to open file %s for writing!\n", path );
        return;
    }

    fprintf( outFile, "#usda 1.0\n\n" );

    for (unsigned go = 0; go < teSceneGetMaxGameObjects(); ++go)
    {
        unsigned sceneGo = teSceneGetGameObjectIndex( scene, go );

        if (sceneGo == 0 || sceneGo == 1 || sceneGo == GetTranslateGizmoGoIndex()) // 0 = unused, 1 = editor camera
        {
            continue;
        }

        fprintf( outFile, "def Xform \"transform%u\"\n{\n", sceneGo );

        Vec3 position = teTransformGetLocalPosition( sceneGo );
        fprintf( outFile, "    double3 xformOp:translate = (%f, %f, %f)\n", position.x, position.y, position.z );

        //Quaternion rotation = teTransformGetLocalRotation( sceneGo );
        //Vec3 rotationv = rotation.get
        //fprintf( outFile, "double3 xformOp:translate = (%f, %f, %f)", position.x, position.y, position.z );

        float* scale = teTransformAccessLocalScale( sceneGo );
        fprintf( outFile, "    float3 xformOp:scale = (%f, %f, %f)\n", *scale, *scale, *scale );
        fprintf( outFile, "    string name = \"%s\"\n", teGameObjectGetName( sceneGo ) );
        
        if ((teGameObjectGetComponents( sceneGo ) & teComponent::MeshRenderer) != 0)
        {
            fprintf( outFile, "    string mesh = \"%s\"\n", teMeshRendererGetMesh( sceneGo )->path );

            for (unsigned m = 0; m < teMeshGetSubMeshCount( teMeshRendererGetMesh( sceneGo ) ); ++m)
            {
                fprintf( outFile, "    string material%u = \"%s\"\n", m, teMeshRendererGetMaterial( sceneGo, m ).name );
            }
        }

        if ((teGameObjectGetComponents( sceneGo ) & teComponent::PointLight) != 0)
        {
            Vec3 lposition, color;
            float radius, intensity;
            tePointLightGetParams( sceneGo, lposition, radius, color, intensity );
            fprintf( outFile, "\n    def SphereLight \"Light\"\n    {\n" );
            fprintf( outFile, "        float inputs:intensity = %f\n", intensity );
            fprintf( outFile, "        color3f inputs:color = (%f, %f, %f)\n", color.x, color.y, color.z );
            fprintf( outFile, "        float inputs:radius = %f\n    }\n", radius );
        }

        const char transformEnd[] = { "}\n\n" };
        fprintf( outFile, transformEnd );
    }

    fclose( outFile );
}
