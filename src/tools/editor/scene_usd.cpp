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
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

unsigned GetTranslateGizmoGoIndex();

void ReadSceneArraySizes( FILE* file, unsigned& outGoCount, unsigned& outTextureCount, 
                          unsigned& outMaterialCount, unsigned& outMeshCount )
{
    outGoCount = 0;
    outTextureCount = 0;
    outMaterialCount = 0;
    outMeshCount = 0;


    char* line;
    size_t len = 0;
    
    while (getline( &line, &len, file ) != -1)
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

    fclose( file );
    
    FILE* file2 = fopen( path, "rb" );
    if (!file2)
    {
        printf( "Unable to open file %s for reading!\n", path );
        return;
    }

    char* line;
    size_t len = 0;

    unsigned goIndex = 0;
    
    while (getline( &line, &len, file2 ) != -1)
    {
        //printf("read line: %s\n", line );
        
        char input[ 255 ];
        sscanf( line, "%254s", input );

        if (strstr( line, "def Xform" ))
        {
            printf("found Xform\n");
            sceneGos[ goIndex ] = teCreateGameObject( "gameobject", teComponent::Transform );
            ++goIndex;
        }
        else if (strstr( line, "def SphereLight" ))
        {
            printf("found SphereLight\n");
        }
        else if (strstr( line, "#usda 1.0" ))
        {
            printf("found usda\n");
        }
    }

    fclose( file2 );
    
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
