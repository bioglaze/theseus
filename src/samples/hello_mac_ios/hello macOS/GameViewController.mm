#import "GameViewController.h"
#include "camera.h"
#include "file.h"
#include "gameobject.h"
#include "material.h"
#include "mesh.h"
#include "renderer.h"
#include "quaternion.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"

void SetDrawable( id< CAMetalDrawable > drawable );
extern MTLRenderPassDescriptor* renderPassDescriptor;
extern id<MTLCommandBuffer> gCommandBuffer;
NSViewController* myViewController;

Vec3 moveDir;

// *Really* minimal PCG32 code / (c) 2014 M.E. O'Neill / pcg-random.org
// Licensed under Apache License 2.0 (NO WARRANTY, etc. see website)
struct pcg32_random_t
{
    uint64_t state;
    uint64_t inc;
};

uint32_t pcg32_random_r( pcg32_random_t* rng )
{
    uint64_t oldstate = rng->state;
    rng->state = oldstate * 6364136223846793005ULL + (rng->inc | 1);
    uint32_t xorshifted = uint32_t( ((oldstate >> 18u) ^ oldstate) >> 27u );
    int32_t rot = oldstate >> 59u;
    return (xorshifted >> rot) | (xorshifted << ((-rot) & 31));
}

pcg32_random_t rng;

int Random100()
{
    return pcg32_random_r( &rng ) % 100;
}

@implementation GameViewController
{
    MTKView* _view;
    dispatch_semaphore_t inflightSemaphore;
    
    teShader unlitShader;
    teShader fullscreenShader;
    
    teGameObject camera3d;
    teGameObject cubeGo;
    teGameObject cubes[ 16 * 4 ];
    teMaterial material;
    teMesh cubeMesh;
    teTexture2D gliderTex;
    teScene scene;
}

void RotateCamera( unsigned index, float x, float y )
{
    teTransformOffsetRotate( index, Vec3( 0, 1, 0 ), -x / 20 );
    teTransformOffsetRotate( index, Vec3( 1, 0, 0 ), -y / 20 );
}

void MoveForward( float amount )
{
    moveDir.z = 0.3f * amount;
}

void MoveRight( float amount )
{
    moveDir.x = 0.3f * amount;
}

void MoveUp( float amount )
{
    moveDir.y = 0.3f * amount;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _view = (MTKView *)self.view;

    _view.device = MTLCreateSystemDefaultDevice();

    if(!_view.device)
    {
        NSLog(@"Metal is not supported on this device");
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
        return;
    }

    _view = (MTKView *)self.view;
    _view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    _view.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
    _view.sampleCount = 1;
    _view.delegate = self;
    
    myViewController = self;
    inflightSemaphore = dispatch_semaphore_create( 2 );
    
    const unsigned width = self.view.bounds.size.width;
    const unsigned height = self.view.bounds.size.height;
    
    teCreateRenderer( 1, NULL, width, height );
    
    fullscreenShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "fullscreenVS", "fullscreenPS" );
    unlitShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "unlitVS", "unlitPS" );
    
    camera3d = teCreateGameObject( "camera3d", teComponent::Transform | teComponent::Camera );
    Vec3 cameraPos = { 0, 0, -10 };
    teTransformSetLocalPosition( camera3d.index, cameraPos );
    teCameraSetProjection( camera3d.index, 45, width / (float)height, 0.1f, 800.0f );
    teCameraSetClear( camera3d.index, teClearFlag::DepthAndColor, Vec4( 1, 0, 0, 1 ) );
    teCameraGetColorTexture( camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::BGRA_sRGB, "camera3d color" );
    teCameraGetDepthTexture( camera3d.index ) = teCreateTexture2D( width, height, teTextureFlags::RenderTexture, teTextureFormat::Depth32F, "camera3d depth" );
    
    gliderTex = teLoadTexture( teLoadFile( "assets/textures/glider_color.tga" ), teTextureFlags::GenerateMips );
    
    material = teCreateMaterial( unlitShader );
    teMaterialSetTexture2D( material, gliderTex, 0 );
    
    cubeMesh = teCreateCubeMesh();
    cubeGo = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
    teMeshRendererSetMesh( cubeGo.index, &cubeMesh );
    teMeshRendererSetMaterial( cubeGo.index, material, 0 );

    scene = teCreateScene();
    teSceneAdd( scene, camera3d.index );
    teSceneAdd( scene, cubeGo.index );
    
    unsigned g = 0;
    Quaternion rotation;

    for (int j = 0; j < 4; ++j)
    {
        for (int i = 0; i < 4; ++i)
        {
            for (int k = 0; k < 4; ++k)
            {
                cubes[ g ] = teCreateGameObject( "cube", teComponent::Transform | teComponent::MeshRenderer );
                teMeshRendererSetMesh( cubes[ g ].index, &cubeMesh );
                teMeshRendererSetMaterial( cubes[ g ].index, material, 0 );
                teTransformSetLocalPosition( cubes[ g ].index, Vec3( i * 4.0f - 5.0f, j * 4.0f - 5.0f, -4.0f - k * 4.0f ) );
                teSceneAdd( scene, cubes[ g ].index );

                float angle = Random100() / 100.0f * 90;
                Vec3 axis{ 1, 1, 1 };
                axis.Normalize();

                rotation.FromAxisAngle( axis, angle );
                teTransformSetLocalRotation( cubes[ g ].index, rotation );
                
                ++g;
            }
        }
    }

    teFinalizeMeshBuffers();
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    dispatch_semaphore_wait( inflightSemaphore, DISPATCH_TIME_FOREVER );

    renderPassDescriptor = _view.currentRenderPassDescriptor;

    float dt = 1;
    
    teTransformMoveForward( camera3d.index, moveDir.z * (float)dt * 0.5f );
    teTransformMoveRight( camera3d.index, moveDir.x * (float)dt * 0.5f );
    teTransformMoveUp( camera3d.index, moveDir.y * (float)dt * 0.5f );

    SetDrawable( view.currentDrawable );
    teBeginFrame();
    teSceneRender( scene, NULL, NULL, NULL );
    teBeginSwapchainRendering( teCameraGetColorTexture( camera3d.index ) );
    teDrawFullscreenTriangle( fullscreenShader, teCameraGetColorTexture( camera3d.index ) );
    teEndSwapchainRendering();
    
    __block dispatch_semaphore_t block_sema = inflightSemaphore;
    [gCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) { dispatch_semaphore_signal( block_sema ); }];

    teEndFrame();
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    
}

- (BOOL)commitEditingAndReturnError:(NSError *__autoreleasing  _Nullable * _Nullable)error
{
    return TRUE;
}

- (void)encodeWithCoder:(nonnull NSCoder *)coder
{
    
}

- (void)mouseDown:(NSEvent *)theEvent
{
    NSLog(@"mouse down\n");
    //inputEvent.x = (int)theEvent.locationInWindow.x;
}

- (void)mouseUp:(NSEvent *)theEvent
{
    NSLog(@"mouse up\n");
    //inputEvent.x = (int)theEvent.locationInWindow.x;
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    RotateCamera( camera3d.index, theEvent.deltaX, theEvent.deltaY );
}

- (void)keyDown:(NSEvent *)theEvent
{
    if ([theEvent keyCode] == 0x00) // A
    {
        MoveRight( 1 );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        MoveRight( -1 );
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        MoveForward( 1 );
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        MoveForward( -1 );
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        MoveUp( 1 );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        MoveUp( -1 );
    }
}

- (void)keyUp:(NSEvent *)theEvent
{
    if ([theEvent keyCode] == 0x00) // A
    {
        MoveRight( 0 );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        MoveRight( 0 );
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        MoveForward( 0 );
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        MoveForward( 0 );
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        MoveUp( 0 );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        MoveUp( 0 );
    }
}

@end
