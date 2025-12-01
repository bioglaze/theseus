#include "file.h"
#include "gameobject.h"
#include "scene.h"
#include "transform.h"
#include "vec3.h"
#include <stdio.h>

void LoadUsdScene( teScene& scene, const char* path )
{
    teFile file = teLoadFile( path );

}

void SaveUsdScene( const teScene& scene, const char* path )
{
    FILE* outFile = fopen( path, "wb" );
    if (!outFile)
    {
        printf( "Unable to open file %s for writing!\n", path );
        return;
    }

    const char header[] = { "#usda 1.0\n\n" };
    //fwrite( header, sizeof( char ), sizeof( header ), outFile );
    fprintf( outFile, "#usda 1.0\n\n" );

    for (unsigned go = 0; go < teSceneGetMaxGameObjects(); ++go)
    {
        unsigned sceneGo = teSceneGetGameObjectIndex( scene, go );

        if (sceneGo == 0)
        {
            continue;
        }

        if ((teGameObjectGetComponents( sceneGo ) & teComponent::MeshRenderer) != 0)
        {
            //continue;
        }

        const char transform[] = { "def Xform \"transform\"\n{\n" };
        //fwrite( transform, sizeof( char ), sizeof( transform ), outFile );
        fprintf( outFile, "def Xform \"transform\"\n{\n" );

        Vec3 position = teTransformGetLocalPosition( sceneGo );
        
        const char transformEnd[] = { "}\n\n" };
        //fwrite( transformEnd, sizeof( char ), sizeof( transformEnd ), outFile );
        fprintf( outFile, transformEnd );

    }

    fclose( outFile );
}
