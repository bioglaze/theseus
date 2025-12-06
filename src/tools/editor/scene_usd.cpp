#include "file.h"
#include "gameobject.h"
#include "light.h"
#include "quaternion.h"
#include "scene.h"
#include "transform.h"
#include "vec3.h"
#include <stdio.h>
#include <string.h>

unsigned GetTranslateGizmoGoIndex();

void LoadUsdScene( teScene& scene, const char* path )
{
    FILE* file = fopen( path, "rb" );
    if (!file)
    {
        printf( "Unable to open file %s for reading!\n", path );
        return;
    }

    char line[ 255 ];

    while (fgets( line, 255, file ) != nullptr)
    {
        char input[ 255 ];
        sscanf( line, "%254s", input );
        
        if (strstr( input, "def Xform" ))
        {
            printf("found Xform\n");
        }
        else if (strstr( input, "def SphereLight" ))
        {
            printf("found SphereLight\n");
        }
        else if (strstr( input, "#usda 1.0" ))
        {
            printf("found usda\n");
        }
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
