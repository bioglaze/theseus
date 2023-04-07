#import "GameViewController.h"
#include "file.h"
#include "renderer.h"
#include "shader.h"

@implementation GameViewController
{
    MTKView *_view;

    teShader unlitShader;
    teShader fullscreenShader;
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
    
    teCreateRenderer( 1, NULL, self.view.bounds.size.width, self.view.bounds.size.height );
    
    fullscreenShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "fullscreenVS", "fullscreenPS" );
    unlitShader = teCreateShader( teLoadFile( "" ), teLoadFile( "" ), "unlitVS", "unlitPS" );
}

- (void)drawInMTKView:(nonnull MTKView *)view {
    
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size {
    
}

- (BOOL)commitEditingAndReturnError:(NSError *__autoreleasing  _Nullable * _Nullable)error {
    return TRUE;
}

- (void)encodeWithCoder:(nonnull NSCoder *)coder {
    
}

@end
