#import "GameViewController.h"

void InitApp( unsigned width, unsigned height );
void DrawApp();
void SetDrawable( id< CAMetalDrawable > drawable );
extern MTLRenderPassDescriptor* renderPassDescriptor;
extern id<MTLCommandBuffer> gCommandBuffer;

@implementation GameViewController
{
    MTKView *_view;
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

    if(!_view.device)
    {
        NSLog(@"Metal is not supported on this device");
        self.view = [[UIView alloc] initWithFrame:self.view.frame];
        return;
    }

    const unsigned width = self.view.bounds.size.width;
    const unsigned height = self.view.bounds.size.height;
        
    InitApp( width, height );

    //_view.delegate = _renderer;
}

@end
