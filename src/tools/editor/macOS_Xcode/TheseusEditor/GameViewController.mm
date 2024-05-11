#import "GameViewController.h"
#import "RRenderer.h"
#include "camera.h"
#include "file.h"
#include "gameobject.h"
#include "imgui.h"
#include "material.h"
#include "mesh.h"
#include "renderer.h"
#include "quaternion.h"
#include "scene.h"
#include "shader.h"
#include "texture.h"
#include "transform.h"
#include "vec3.h"
#include <stdint.h>

void InitSceneView( unsigned width, unsigned height, void* windowHandle, int uiScale );

@implementation GameViewController
{
    MTKView* _view;

    Renderer* _renderer;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _view = (MTKView *)self.view;
    _view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    _view.depthStencilPixelFormat = MTLPixelFormatDepth32Float;

    _view.device = MTLCreateSystemDefaultDevice();

    if(!_view.device)
    {
        NSLog(@"Metal is not supported on this device");
        self.view = [[NSView alloc] initWithFrame:self.view.frame];
        return;
    }

    _renderer = [[Renderer alloc] initWithMetalKitView:_view];

    [_renderer mtkView:_view drawableSizeWillChange:_view.drawableSize];

    _view.delegate = _renderer;

    const unsigned width = _view.bounds.size.width;
    const unsigned height = _view.bounds.size.height;
    InitSceneView( width, height, nullptr, 2 );
}

@end
