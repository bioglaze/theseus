#import <Metal/Metal.h>
#import <AppKit/AppKit.h>
#import <MetalKit/MetalKit.h>
#import <QuartzCore/CAMetalLayer.h>

extern id<CAMetalDrawable> gDrawable;
extern MTLRenderPassDescriptor* renderPassDescriptor;

void Init( unsigned width, unsigned height );
void Render();

unsigned width = 800, height = 450;

@interface GameView : MTKView
@end

@implementation GameView
- (id)initWithFrame:(CGRect)inFrame
{
    id<MTLDevice> device = MTLCreateSystemDefaultDevice();
    self = [super initWithFrame:inFrame device:device];
    if (self)
    {
        self.colorPixelFormat = MTLPixelFormatBGRA8Unorm_sRGB;
        self.depthStencilPixelFormat = MTLPixelFormatDepth32Float;
        
        Init( width, height );
    }
    return self;
}

- (void)drawRect:(CGRect)rect
{
    renderPassDescriptor = self.currentRenderPassDescriptor;
    gDrawable = self.currentDrawable;
    Render();
}
@end

@interface KeyHandlingWindow: NSWindow
@end

@implementation KeyHandlingWindow

- (void)keyDown:(NSEvent *)theEvent
{
    if ([theEvent keyCode] == 0x00) // A
    {
        //MoveRight( -1 );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        //MoveRight( 1 );
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        //MoveForward( 1 );
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        //MoveForward( -1 );
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        //MoveUp( -1 );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        //MoveUp( 1 );
    }
}

- (void)keyUp:(NSEvent *)theEvent
{
    if ([theEvent keyCode] == 0x00) // A
    {
        //MoveRight( 0 );
    }
    else if ([theEvent keyCode] == 0x02) // D
    {
        //MoveRight( 0 );
    }
    else if ([theEvent keyCode] == 0x0D) // W
    {
        //MoveForward( 0 );
    }
    else if ([theEvent keyCode] == 0x01) // S
    {
        //MoveForward( 0 );
    }
    else if ([theEvent keyCode] == 0x0C) // Q
    {
        //MoveUp( 0 );
    }
    else if ([theEvent keyCode] == 0x0E) // E
    {
        //MoveUp( 0 );
    }
}

- (void)mouseDown:(NSEvent *)event
{
    int x = (int)event.locationInWindow.x;
    int y = height - (int)event.locationInWindow.y;
}

- (void)mouseUp:(NSEvent *)theEvent
{
    //MouseUp( (int)theEvent.locationInWindow.x, (int)theEvent.locationInWindow.y );
}

- (void)mouseMoved:(NSEvent *)theEvent
{
    //MouseMove( (int)theEvent.locationInWindow.x, self.view.bounds.size.height - (int)theEvent.locationInWindow.y );
}

- (void)mouseDragged:(NSEvent *)theEvent
{
    //teTransformOffsetRotate( m_camera3d.index, Vec3( 0, 1, 0 ), -theEvent.deltaX / 20.0f );
    //teTransformOffsetRotate( m_camera3d.index, Vec3( 1, 0, 0 ), -theEvent.deltaY / 20.0f );
}

@end

int main()
{
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];
        
        NSMenu* bar = [NSMenu new];
        NSMenuItem * barItem = [NSMenuItem new];
        NSMenu* menu = [NSMenu new];
        NSMenuItem* quit = [[NSMenuItem alloc]
                            initWithTitle:@"Quit"
                            action:@selector(terminate:)
                            keyEquivalent:@"q"];
        [bar addItem:barItem];
        [barItem setSubmenu:menu];
        [menu addItem:quit];
        NSApp.mainMenu = bar;
        
        NSRect rect = NSMakeRect(0, 0, width, height);
        NSRect frame = NSMakeRect(0, 0, width, height);
        NSWindow* window = [[KeyHandlingWindow alloc]
                            initWithContentRect:rect
                            styleMask:NSWindowStyleMaskTitled
                            backing:NSBackingStoreBuffered
                            defer:NO];
        [window cascadeTopLeftFromPoint:NSMakePoint(20,20)];
        window.styleMask |= NSWindowStyleMaskResizable;
        window.styleMask |= NSWindowStyleMaskMiniaturizable ;
        window.styleMask |= NSWindowStyleMaskClosable;
        window.title = [[NSProcessInfo processInfo] processName];
        [window makeKeyAndOrderFront:nil];
        
        GameView* view = [[GameView alloc] initWithFrame:frame];
        window.contentView = view;
        
        [NSApp run];
    }
    
    return 0;
}
