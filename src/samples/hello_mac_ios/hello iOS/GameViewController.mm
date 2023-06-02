#import "GameViewController.h"
#include "renderer.h"

void InitApp( unsigned width, unsigned height );
void DrawApp();
void SetDrawable( id< CAMetalDrawable > drawable );
extern MTLRenderPassDescriptor* renderPassDescriptor;
extern id<MTLCommandBuffer> gCommandBuffer;

@implementation GameViewController
{
    MTKView *_view;
    dispatch_semaphore_t inflightSemaphore;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    _view = (MTKView *)self.view;

    _view.device = MTLCreateSystemDefaultDevice();
    _view.backgroundColor = UIColor.blackColor;
    _view.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
    _view.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
    _view.sampleCount = 1;
    _view.delegate = self;
    
    if(!_view.device)
    {
        NSLog(@"Metal is not supported on this device");
        self.view = [[UIView alloc] initWithFrame:self.view.frame];
        return;
    }

    const unsigned width = self.view.bounds.size.width;
    const unsigned height = self.view.bounds.size.height;
        
    InitApp( width, height );

    inflightSemaphore = dispatch_semaphore_create( 2 );
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    @autoreleasepool
    {
        dispatch_semaphore_wait( inflightSemaphore, DISPATCH_TIME_FOREVER );

        renderPassDescriptor = _view.currentRenderPassDescriptor;

        SetDrawable( view.currentDrawable );

        DrawApp();
            
        __block dispatch_semaphore_t block_sema = inflightSemaphore;
        [gCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer) { dispatch_semaphore_signal( block_sema ); }];

        teEndFrame();
    }
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
}

- (void)encodeWithCoder:(nonnull NSCoder *)aCoder
{
}

@end
