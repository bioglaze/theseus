#include "camera.h"
#include "imgui.h"
#include "file.h"
#include "gameobject.h"
#include "mesh.h"
#include "renderer.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"
#include "window.h"

int main()
{
    constexpr unsigned width = 1920;
    constexpr unsigned height = 1080;
    void* windowHandle = teCreateWindow( width, height, "Theseus Engine Hello" );
    teCreateRenderer( 1, windowHandle, width, height );

    teFile unlitVsFile = teLoadFile( "shaders/unlit_vs.spv" );
    teFile unlitFsFile = teLoadFile( "shaders/unlit_fs.spv" );
    teShader unlitShader = teCreateShader( unlitVsFile, unlitFsFile, "unlitVS", "unlitFS" );
    teTexture2D cameraColorTex = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
    teTexture2D cameraDepthTex = teCreateTexture2D( width, height, 0, teTextureFormat::Depth32F, "camera3d depth" );

    teGameObject camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
    Vec3 cameraPos = { 0, 0, -10 };
    teTransformSetLocalPosition( camera3d.index, cameraPos );
    teCameraSetProjection( camera3d.index, 45, 1280.0f / 720.0f, 0.1f, 800.0f );

    teMesh cubeMesh = teCreateCubeMesh();
    teGameObject cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );

    teScene scene = teCreateScene();
    teSceneAdd( scene, camera3d.index );
    teSceneAdd( scene, cubeGo.index );

    ImGuiContext* imContext = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    bool shouldQuit = false;

    while (!shouldQuit)
    {
        tePushWindowEvents();

        bool eventsHandled = false;

        while (!eventsHandled)
        {
            const teWindowEvent& event = tePopWindowEvent();

            if (event.type == teWindowEvent::Type::Empty)
            {
                eventsHandled = true;
            }

            if (event.type == teWindowEvent::Type::KeyDown || event.type == teWindowEvent::Type::Close)
            {
                shouldQuit = true;
            }
        }

        teBeginFrame();
        teEndFrame();
    }

    delete[] unlitVsFile.data;
    delete[] unlitFsFile.data;

    ImGui::DestroyContext( imContext );

    return 0;
}
