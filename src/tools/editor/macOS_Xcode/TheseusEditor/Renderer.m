#import "RRenderer.h"
#include "vec3.h"

void RenderSceneView();
void SetDrawable( id< CAMetalDrawable > drawable );
void MoveEditorCamera( float right, float up, float forward );
extern MTLRenderPassDescriptor* renderPassDescriptor;
extern id<MTLCommandBuffer> gCommandBuffer;
extern Vec3 moveDir;

static const NSUInteger kMaxBuffersInFlight = 3;

@implementation Renderer
{
    dispatch_semaphore_t _inFlightSemaphore;
}

-(nonnull instancetype)initWithMetalKitView:(nonnull MTKView *)view;
{
    self = [super init];
    if(self)
    {
        _inFlightSemaphore = dispatch_semaphore_create(kMaxBuffersInFlight);
        [self _loadMetalWithView:view];
    }

    return self;
}

- (void)_loadMetalWithView:(nonnull MTKView *)view;
{
}

- (void)_updateGameState
{
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    dispatch_semaphore_wait(_inFlightSemaphore, DISPATCH_TIME_FOREVER);

    SetDrawable( view.currentDrawable );

    __block dispatch_semaphore_t block_sema = _inFlightSemaphore;
    [gCommandBuffer addCompletedHandler:^(id<MTLCommandBuffer> buffer)
     {
         dispatch_semaphore_signal(block_sema);
     }];

    renderPassDescriptor = view.currentRenderPassDescriptor;

    RenderSceneView();
    MoveEditorCamera( moveDir.x, moveDir.y, moveDir.z );
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    /// Respond to drawable size or orientation changes here
}

@end
