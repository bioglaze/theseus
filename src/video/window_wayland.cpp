#include "window.h"
#include "te_stdlib.h"
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <libdecor.h>
#include <wayland-client.h>
#include <wayland-cursor.h>
#include <linux/input.h>
#include <linux/joystick.h>

// TODO: rename link, conflicts with some header

constexpr int EventStackSize = 100;

struct GamePad
{
    bool isActive = false;
    int fd;
    int buttonA;
    int buttonB;
    int buttonX;
    int buttonY;
    int buttonBack;
    int buttonStart;
    int buttonLeftShoulder;
    int buttonRightShoulder;
    int dpadXaxis;
    int dpadYaxis;
    int leftThumbX;
    int leftThumbY;
    int rightThumbX;
    int rightThumbY;
    short deadZone;

    float lastLeftThumbX = 0;
    float lastLeftThumbY = 0;
    float lastRightThumbX = 0;
    float lastRightThumbY = 0;
};

struct WindowImpl
{
    teWindowEvent          events[ EventStackSize ];
    teWindowEvent::KeyCode keyMap[ 256 ] = {};
    int                    eventIndex = -1;
    unsigned               width = 0;
    unsigned               height = 0;

    // FIXME: this is a hack to get mouse coordinate into mouse move event.
    //       A proper fix would be to read mouse coordinate when mouse move event is detected.
    unsigned              lastMouseX = 0;
    unsigned              lastMouseY = 0;

    GamePad               gamePad;
};

WindowImpl win;

void IncEventIndex()
{
    if (win.eventIndex < EventStackSize - 1)
    {
        ++win.eventIndex;
    }
}

static float ProcessGamePadStickValue( short value, short deadZoneThreshold )
{
    float result = 0;
        
    if (value < -deadZoneThreshold)
    {
        result = (value + deadZoneThreshold) / (32768.0f - deadZoneThreshold);
    }
    else if (value > deadZoneThreshold)
    {
        result = (value - deadZoneThreshold) / (32767.0f - deadZoneThreshold);
    }

    return result;
}

wl_compositor* wlCompositor;
wl_list seats;
//wl_list gLink;
wl_list outputs;
wl_shm* wlShm;
const char* proxyTag = "theseus";
struct xdg_wm_base* xdgWmBase;

struct Output
{
    wl_output* wlOutput;
    uint32_t   id;
    int        scale;
    wl_list    link;
};

struct WindowOutput
{
    Output*        output;
    struct wl_list link;
};

struct pointer_output
{
    Output*        output;
    struct wl_list link;
};

struct WlBuffer
{
    wl_buffer* wlBuffer;
    void*      data;
    size_t     dataSize;
};

struct Seat
{
    wl_seat*         wlSeat;
    wl_keyboard*     wlKeyboard;
    wl_pointer*      wlPointer;
    wl_list          link;
    wl_list          pointer_outputs;
    wl_cursor_theme* cursor_theme;
    wl_cursor*       left_ptr_cursor;
    wl_surface*      cursor_surface;
    wl_surface*      pointer_focus;
    int              pointer_scale;
    uint32_t         serial;
    wl_fixed_t       pointer_sx;
    wl_fixed_t       pointer_sy;
    char*            name;
};

Seat seat;

struct Window
{
    wl_surface*     wlSurface;
    libdecor_frame* frame;
    wl_list         outputs;
    int             scale = 1;
    bool            isConfigured = false;
    int             outputScale = 1;
};

Window window;

wl_surface* gwlSurface;
wl_display* gwlDisplay;

void updateScale( Window* windowa );

extern const struct wl_interface wl_output_interface;
extern const struct wl_interface wl_seat_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface xdg_popup_interface;
extern const struct wl_interface xdg_positioner_interface;
extern const struct wl_interface xdg_surface_interface;
extern const struct wl_interface xdg_toplevel_interface;

static const struct wl_interface *xdg_shell_types[] = {
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &xdg_positioner_interface,
    &xdg_surface_interface,
    &wl_surface_interface,
    &xdg_toplevel_interface,
    &xdg_popup_interface,
    &xdg_surface_interface,
    &xdg_positioner_interface,
    &xdg_toplevel_interface,
    &wl_seat_interface,
    nullptr,
    nullptr,
    nullptr,
    &wl_seat_interface,
    nullptr,
    &wl_seat_interface,
    nullptr,
    nullptr,
    &wl_output_interface,
    &wl_seat_interface,
    nullptr,
    &xdg_positioner_interface,
    nullptr,
};

static const wl_message xdg_wm_base_requests[] = {
    { "destroy", "", xdg_shell_types + 0 },
    { "create_positioner", "n", xdg_shell_types + 4 },
    { "get_xdg_surface", "no", xdg_shell_types + 5 },
    { "pong", "u", xdg_shell_types + 0 },
};

static const wl_message xdg_wm_base_events[] = {
    { "ping", "u", xdg_shell_types + 0 },
};

const wl_interface xdg_wm_base_interface = {
    "xdg_wm_base", 4,
    4, xdg_wm_base_requests,
    1, xdg_wm_base_events,
};

static const wl_message xdg_toplevel_requests[] = {
	{ "destroy", "", xdg_shell_types + 0 },
	{ "set_parent", "?o", xdg_shell_types + 11 },
	{ "set_title", "s", xdg_shell_types + 0 },
	{ "set_app_id", "s", xdg_shell_types + 0 },
	{ "show_window_menu", "ouii", xdg_shell_types + 12 },
	{ "move", "ou", xdg_shell_types + 16 },
	{ "resize", "ouu", xdg_shell_types + 18 },
	{ "set_max_size", "ii", xdg_shell_types + 0 },
	{ "set_min_size", "ii", xdg_shell_types + 0 },
	{ "set_maximized", "", xdg_shell_types + 0 },
	{ "unset_maximized", "", xdg_shell_types + 0 },
	{ "set_fullscreen", "?o", xdg_shell_types + 21 },
	{ "unset_fullscreen", "", xdg_shell_types + 0 },
	{ "set_minimized", "", xdg_shell_types + 0 },
};

static const wl_message xdg_toplevel_events[] = {
    { "configure", "iia", xdg_shell_types + 0 },
    { "close", "", xdg_shell_types + 0 },
    { "configure_bounds", "4ii", xdg_shell_types + 0 },
};

const wl_interface xdg_toplevel_interface = {
	"xdg_toplevel", 4,
	14, xdg_toplevel_requests,
	3, xdg_toplevel_events,
};

static const wl_message xdg_popup_requests[] = {
	{ "destroy", "", xdg_shell_types + 0 },
	{ "grab", "ou", xdg_shell_types + 22 },
	{ "reposition", "3ou", xdg_shell_types + 24 },
};

static const wl_message xdg_popup_events[] = {
	{ "configure", "iiii", xdg_shell_types + 0 },
	{ "popup_done", "", xdg_shell_types + 0 },
	{ "repositioned", "3u", xdg_shell_types + 0 },
};

const wl_interface xdg_popup_interface = {
	"xdg_popup", 4,
	3, xdg_popup_requests,
	3, xdg_popup_events,
};

static const struct wl_message xdg_surface_requests[] = {
    { "destroy", "", xdg_shell_types + 0 },
    { "get_toplevel", "n", xdg_shell_types + 7 },
    { "get_popup", "n?oo", xdg_shell_types + 8 },
    { "set_window_geometry", "iiii", xdg_shell_types + 0 },
    { "ack_configure", "u", xdg_shell_types + 0 },
};

static const struct wl_message xdg_surface_events[] = {
    { "configure", "u", xdg_shell_types + 0 },
};

const struct wl_interface xdg_surface_interface = {
    "xdg_surface", 4,
    5, xdg_surface_requests,
    1, xdg_surface_events,
};

static const struct wl_message xdg_positioner_requests[] = {
	{ "destroy", "", xdg_shell_types + 0 },
	{ "set_size", "ii", xdg_shell_types + 0 },
	{ "set_anchor_rect", "iiii", xdg_shell_types + 0 },
	{ "set_anchor", "u", xdg_shell_types + 0 },
	{ "set_gravity", "u", xdg_shell_types + 0 },
	{ "set_constraint_adjustment", "u", xdg_shell_types + 0 },
	{ "set_offset", "ii", xdg_shell_types + 0 },
	{ "set_reactive", "3", xdg_shell_types + 0 },
	{ "set_parent_size", "3ii", xdg_shell_types + 0 },
	{ "set_parent_configure", "3u", xdg_shell_types + 0 },
};

const struct wl_interface xdg_positioner_interface = {
	"xdg_positioner", 4,
	10, xdg_positioner_requests,
	0, NULL,
};

static void shm_format( void *data, struct wl_shm *wl_shm, uint32_t format )
{
    if (format == WL_SHM_FORMAT_XRGB8888)
    {
        //has_xrgb = true;
        printf( "Has xrgb\n" );
    }
}

static struct wl_shm_listener shm_listener = { shm_format };

bool ownProxy( wl_proxy* proxy )
{
    return wl_proxy_get_tag( proxy ) == &proxyTag;
}

bool ownOutput( wl_output* output )
{
    return ownProxy( (wl_proxy*)output );
}

static void cursorSurfaceEnter( void* data, wl_surface* wlSurface, wl_output* wlOutput )
{
    Seat *seate = (Seat*)data;
    pointer_output* pointerOutput;

    if (!ownOutput( wlOutput ))
        return;

    pointerOutput = (pointer_output*)malloc( sizeof( pointer_output ) );
    pointerOutput->output = (Output*)wl_output_get_user_data( wlOutput );
    wl_list_insert( &seate->pointer_outputs, &pointerOutput->link );
    //try_update_cursor( seate );
    printf( "TODO: try_update_cursor()\n");
}

static void cursorSurfaceLeave( void* data, wl_surface* wlSurface, wl_output* wlOutput )
{
    Seat* seate = (Seat*)data;
    pointer_output* pointerOutput, *tmp;

    wl_list_for_each_safe( pointerOutput, tmp, &seate->pointer_outputs, link )
    {
        if (pointerOutput->output->wlOutput == wlOutput)
        {
            wl_list_remove( &pointerOutput->link );
            free( pointerOutput );
        }
    }
}

static wl_surface_listener cursorSurfaceListener = { cursorSurfaceEnter, cursorSurfaceLeave };

static void initCursors( Seat *pseat )
{
    char* name = nullptr;
    int size = 0;

    // FIXME: this method doesn't exist?
    //if (!libdecor_get_cursor_settings( &name, &size ))
    {
        name = nullptr;
        size = 48; // FIXME: this should maybe be multiplied by window scale, but it's 1 at this point, even if should be 2 on HiDPI?
    }
    size *= pseat->pointer_scale;

    wl_cursor_theme* theme = wl_cursor_theme_load( name, size, wlShm );
    free( name );

    if (theme != nullptr)
    {
        if (pseat->cursor_theme)
        {
            wl_cursor_theme_destroy( pseat->cursor_theme );
        }
        
        pseat->cursor_theme = theme;
    }
    if (pseat->cursor_theme)
    {
        pseat->left_ptr_cursor = wl_cursor_theme_get_cursor( pseat->cursor_theme, "left_ptr" );
    }
    if (!pseat->cursor_surface)
    {
        pseat->cursor_surface = wl_compositor_create_surface( wlCompositor );
        wl_surface_add_listener( pseat->cursor_surface, &cursorSurfaceListener, pseat );
    }
}

static void setCursor( Seat* pseat )
{
    const int scale = pseat->pointer_scale;

    if (!pseat->cursor_theme)
        return;

    wl_cursor* wlCursor = pseat->left_ptr_cursor;

    wl_cursor_image* image = wlCursor->images[ 0 ];
    wl_buffer* buffer = wl_cursor_image_get_buffer( image );
    wl_pointer_set_cursor(pseat->wlPointer, pseat->serial,
                          pseat->cursor_surface,
                          image->hotspot_x / scale,
                          image->hotspot_y / scale);
    wl_surface_attach( pseat->cursor_surface, buffer, 0, 0 );
    wl_surface_set_buffer_scale( pseat->cursor_surface, scale );
    wl_surface_damage_buffer( pseat->cursor_surface, 0, 0,
                              image->width, image->height );
                              wl_surface_commit( pseat->cursor_surface );
}

void pointerEnter( void*       data,
                   wl_pointer* wlPointer,
                   uint32_t    serial,
                   wl_surface* surface,
                   wl_fixed_t  surface_x,
                   wl_fixed_t  surface_y )
{
    Seat *dseat = (Seat*)data;

    dseat->pointer_focus = surface;
    dseat->serial = serial;

    if (surface != window.wlSurface)
        return;

    printf( "pointer_enter\n" );
    setCursor( dseat );

    dseat->pointer_sx = surface_x;
    dseat->pointer_sy = surface_y;
}

void pointerButton( void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t pointerState )
{
    if (button == BTN_LEFT) // BTN_LEFT = 272, BTN_RIGHT: 273, BTN_MIDDLE: 274
    {
        bool pressed = pointerState == WL_POINTER_BUTTON_STATE_PRESSED;
        printf( "pressed left button: %d\n", pressed );
    }

    IncEventIndex();

    if (button == BTN_LEFT)
    {
        win.events[ win.eventIndex ].type = pointerState == WL_POINTER_BUTTON_STATE_PRESSED ? teWindowEvent::Type::Mouse1Down : teWindowEvent::Type::Mouse1Up;
    }
    else if (button == BTN_RIGHT)
    {
        win.events[ win.eventIndex ].type = pointerState == WL_POINTER_BUTTON_STATE_PRESSED ? teWindowEvent::Type::Mouse2Down : teWindowEvent::Type::Mouse2Up;
    }
    else if (button == BTN_MIDDLE)
    {
        win.events[ win.eventIndex ].type = pointerState == WL_POINTER_BUTTON_STATE_PRESSED ? teWindowEvent::Type::Mouse3Down : teWindowEvent::Type::Mouse3Up;
    }
    
    win.events[ win.eventIndex ].x = win.lastMouseX;
    win.events[ win.eventIndex ].y = win.lastMouseY;
}

void pointerMotion( void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y )
{
    IncEventIndex();
    win.events[ win.eventIndex ].type = teWindowEvent::Type::MouseMove;
    win.events[ win.eventIndex ].x = (float)wl_fixed_to_double( x );
    win.events[ win.eventIndex ].y = (float)wl_fixed_to_double( y );
    
    win.lastMouseX = win.events[ win.eventIndex ].x;
    win.lastMouseY = win.events[ win.eventIndex ].y;
}

void pointerLeave( void* data, wl_pointer* wlPointer, uint32_t serial, wl_surface* surface )
{
    Seat* dseat = (Seat*)data;

    if (dseat->pointer_focus == surface)
        dseat->pointer_focus = nullptr;
}

static void pointerAxis( void*       data,
                         wl_pointer* wlPointer,
                         uint32_t    time,
                         uint32_t    axis,
                         wl_fixed_t  value )
{
}

static struct wl_pointer_listener pointerListener = { pointerEnter, pointerLeave, pointerMotion, pointerButton, pointerAxis, nullptr, nullptr, nullptr, nullptr, nullptr };

void keyMap( void* data, wl_keyboard* wlKeyboard, uint format, int fd, uint size )
{
    printf("keymap\n");
}

void keyEnter( void* data, wl_keyboard* wlKeyboard, uint serial, wl_surface* surface, wl_array* keys )
{
    printf("keyEnter\n");
}

void keyLeave( void* data, wl_keyboard* wlKeyboard, uint serial, wl_surface* surface )
{
    printf("keyLeave\n");
}

void keyKey( void* data, wl_keyboard* wlKeyboard, uint serial, uint time, uint key, uint keyState )
{
    if (keyState == 1)
        printf( "keyKey %u state %u\n", key, keyState );

    IncEventIndex();
    win.events[ win.eventIndex ].type = keyState == 1 ? teWindowEvent::Type::KeyDown : teWindowEvent::Type::KeyUp;
    win.events[ win.eventIndex ].keyCode = win.keyMap[ key ];
}

void keyModifiers( void* data, wl_keyboard* wlKeyboard, uint serial, uint modsDepressed, uint modsLatched, uint modsLocked, uint group )
{
    printf("keyModifiers\n");
}

void keyRepeat( void* data, wl_keyboard* wlKeyboard, int rate, int delay )
{
    printf("keyRepeat\n");
}

wl_keyboard_listener keyboardListener = { &keyMap, &keyEnter, &keyLeave, &keyKey, &keyModifiers, &keyRepeat };

static void seatCapabilities( void* data, wl_seat* wlSeat, uint32_t capabilities )
{
    Seat* dseat = (Seat*)data;
    
    if (capabilities & WL_SEAT_CAPABILITY_POINTER && !dseat->wlPointer)
    {
        dseat->wlPointer = wl_seat_get_pointer( wlSeat );
        wl_pointer_add_listener( dseat->wlPointer, &pointerListener, dseat );
        dseat->pointer_scale = 1;
        initCursors( dseat );
    }
    else if (!(capabilities & WL_SEAT_CAPABILITY_POINTER) && dseat->wlPointer)
    {
        wl_pointer_release( dseat->wlPointer );
        dseat->wlPointer = nullptr;
    }

    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD && !dseat->wlKeyboard)
    {
        dseat->wlKeyboard = wl_seat_get_keyboard( wlSeat );
        wl_keyboard_add_listener( dseat->wlKeyboard, &keyboardListener, dseat );
    }
    else if (!(capabilities & WL_SEAT_CAPABILITY_KEYBOARD) && dseat->wlKeyboard)
    {
        wl_keyboard_release( dseat->wlKeyboard );
        dseat->wlKeyboard = nullptr;
    }
}

static void seatName( void* data, wl_seat* wlSeat, const char* name )
{
    Seat* dseat = (Seat*)data;

    dseat->name = strdup( name );
}

static wl_seat_listener seatListener = { seatCapabilities, seatName };

static void outputGeometry( void* data,
                            struct wl_output *wl_output,
                            int32_t x,
                            int32_t y,
                            int32_t physical_width,
                            int32_t physical_height,
                            int32_t subpixel,
                            const char* make,
                            const char* model,
                            int32_t transform )
{
}

static void outputMode( void* data, wl_output* wlOutput, uint32_t flags, int32_t width, int32_t height, int32_t refresh )
{
    printf( "TODO: output_mode\n" );
}

static void outputDone( void *data, struct wl_output* wl_output )
{
    struct Output *output = (Output*)data;
    struct Seat *sseat;

    //if (window)
    {
        if (output->scale != window.scale)
        {
            //printf("TODO: update scale %d\n", output->scale );
            updateScale( &window );
        }
    }

    wl_list_for_each( sseat, &seats, link )
    {
        printf( "TODO: try_update_cursor\n" );
        //try_update_cursor( seat );
    }
}

static void outputScale( void *data, struct wl_output *wl_output, int32_t factor )
{
    Output* output = (Output*)data;
    output->scale = factor;
    window.outputScale = factor;
    printf( "outputScale: %d\n", factor );
}

static struct wl_output_listener output_listener = { outputGeometry, outputMode, outputDone, outputScale, nullptr, nullptr };

struct xdg_wm_base_listener
{
    void (*ping)( void* data, xdg_wm_base* xdg_wmbase, uint32_t serial );
};

static struct xdg_wm_base_listener xdg_wm_base_listener = {};

static inline int xdg_wm_base_add_listener( xdg_wm_base* xdg_wm_base2, const struct xdg_wm_base_listener* listener, void* data )
{
    return wl_proxy_add_listener( (wl_proxy*) xdg_wm_base2, (void (**)(void)) listener, data );
}

void registry_handle_global( void* userData, struct wl_registry* wl_registry, uint32_t id, const char* interface, uint32_t version )
{
    if (strcmp( interface, "wl_compositor" ) == 0)
    {
        if (version < 4)
        {
            fprintf( stderr, "wl_compositor version >= 4 required" );
            exit( EXIT_FAILURE );
        }
        
        wlCompositor = (wl_compositor*)wl_registry_bind( wl_registry, id, &wl_compositor_interface, 4 );
    }
    else if (strcmp( interface, "wl_shm" ) == 0)
    {
        wlShm = (wl_shm*)wl_registry_bind( wl_registry, id, &wl_shm_interface, 1 );
        wl_shm_add_listener( wlShm, &shm_listener, nullptr );
    }
    else if (strcmp( interface, "wl_seat" ) == 0)
    {
        if (version < 3)
        {
            fprintf( stderr, "%s version 3 required but only version %u is available\n", interface, version );
            exit( EXIT_FAILURE );
        }
        //seat = zalloc(sizeof *seat);
        wl_list_init( &seat.pointer_outputs );
        seat.wlSeat = (wl_seat*)wl_registry_bind( wl_registry, id, &wl_seat_interface, 3 );
        wl_seat_add_listener( seat.wlSeat, &seatListener, &seat );
    }
    else if (strcmp( interface, "wl_output" ) == 0)
    {
        if (version < 2)
        {
            fprintf( stderr, "%s version 3 required but only version %u is available\n", interface, version );
            exit( EXIT_FAILURE );
		}

        Output* output = (Output*)malloc( sizeof( Output ) );
        output->id = id;
        output->scale = 1;
        output->wlOutput = (wl_output*)wl_registry_bind( wl_registry, id, &wl_output_interface, 2 );
        wl_proxy_set_tag( (wl_proxy*) output->wlOutput, &proxyTag );
        wl_output_add_listener( output->wlOutput, &output_listener, output );
        wl_list_insert( &outputs, &output->link );
    }
    else if (strcmp( interface, "xdg_wm_base" ) == 0)
    {
        xdgWmBase = (xdg_wm_base*)wl_registry_bind( wl_registry, id, &xdg_wm_base_interface, 1 );
        xdg_wm_base_add_listener( xdgWmBase, &xdg_wm_base_listener, nullptr );
	}
    
    printf( "registry handle interface: %s\n", interface );
}

void registry_handle_global_remove( void* data, struct wl_registry* registry, uint32_t name )
{
    Output* output;
    WindowOutput* window_output;

    wl_list_for_each( output, &outputs, link )
    {
        if (output->id == name)
        {
            wl_list_for_each( window_output, &outputs, link )
            {
                if (window_output->output == output)
                {
                    wl_list_remove( &window_output->link );
                    free( window_output );
                }
            }

            wl_list_remove( &output->link );
            wl_output_destroy( output->wlOutput );
            free( output );
            break;
        }
    }
}

void updateScale( Window* windowa )
{
    printf( "TODO: updateScale\n");
    
    /*int scale = 1;
    WindowOutput* window_output;

    wl_list_for_each( window_output, &window.outputs, link )
    {
        scale = scale > window_output->output->scale ? scale : window_output->output->scale;
    }
    
    if (scale != window.scale)
    {
        window.scale = scale;
        }*/
}

void surfaceEnter( void* data, wl_surface* wlSurface, wl_output* wlOutput )
{
    //struct window *window = (window*)data;

    if (!ownOutput( wlOutput ))
        return;

    Output* outpu = (Output*)wl_output_get_user_data( wlOutput );

    if (outpu == nullptr)
		return;

    WindowOutput* windowOutput = (WindowOutput*)malloc( sizeof( WindowOutput ) ); // FIXME: Does this need to be dynamically allocated?
    windowOutput->output = outpu;

    // FIXME: why does the program crash if the following wl_list_insert is executed? Is it even necessary?
    //wl_list_insert( &window.outputs, &windowOutput->link );
    
    updateScale( &window );
}

// FIXME: Got a random crash in this function on startup, maybe related to mouse position and something hasn't been initialized yet?
void surfaceLeave( void* data, wl_surface* wlSurface, wl_output* wlOutput )
{
    Window* myWindow = (Window*)data;
    WindowOutput* window_output;

    wl_list_for_each( window_output, &myWindow->outputs, link )
    {
        if (window_output->output->wlOutput == wlOutput)
        {
            wl_list_remove( &window_output->link );
            //free( window_output );
            updateScale( &window );
            break;
        }
    }
}

static void handleConfigure( libdecor_frame* frame, libdecor_configuration* configuration, void* user_data )
{
    libdecor_window_state windowState = LIBDECOR_WINDOW_STATE_NONE;
    
    if (!libdecor_configuration_get_window_state( configuration, &windowState ) )
    {
        windowState = LIBDECOR_WINDOW_STATE_NONE;
    }

    int width = 0, height = 0;
    
    if (!libdecor_configuration_get_content_size( configuration, frame, &width, &height ))
    {
        printf( "handle_configure 1: width: %d, height %d. scale: %d, output scale: %d\n", width, height, window.scale, window.outputScale );
        width = win.width;
        height = win.height;
    }

    printf( "handle_configure 2: width: %d, height %d\n", width, height );

    libdecor_state* decState = libdecor_state_new( width, height );
	libdecor_frame_commit( frame, decState, configuration );
	libdecor_state_free( decState );

    window.isConfigured = true;
}

void handle_error( libdecor* context, enum libdecor_error error, const char* message )
{
    printf( "libdecor error: %d: %s\n", error, message );
}

static struct libdecor_interface libdecor_iface =
{
    handle_error, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

static void handleClose( libdecor_frame* frame, void* user_data )
{
    exit( EXIT_SUCCESS );
}

static void handleCommit( libdecor_frame* frame, void* user_data )
{
    printf("handle_commit\n");
    wl_surface_commit( window.wlSurface );
}

static void handle_dismiss_popup( libdecor_frame* frame, const char* seatName, void* userData )
{
}

libdecor* decorContext;
libdecor_frame_interface decorFrameInterface = { handleConfigure, handleClose, handleCommit, handle_dismiss_popup, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

wl_surface_listener surfaceListener = { surfaceEnter, surfaceLeave };
wl_registry_listener registryListener = { registry_handle_global, registry_handle_global_remove };

static void InitKeyMap()
{
    for (unsigned keyIndex = 0; keyIndex < 256; ++keyIndex)
    {
        win.keyMap[ keyIndex ] = teWindowEvent::KeyCode::None;
    }

    win.keyMap[ 28 ] = teWindowEvent::KeyCode::Enter;
    win.keyMap[ 105 ] = teWindowEvent::KeyCode::Left;
    win.keyMap[ 103 ] = teWindowEvent::KeyCode::Up;
    win.keyMap[ 106 ] = teWindowEvent::KeyCode::Right;
    win.keyMap[ 108 ] = teWindowEvent::KeyCode::Down;
    win.keyMap[  1 ] = teWindowEvent::KeyCode::Escape;
    win.keyMap[ 57 ] = teWindowEvent::KeyCode::Space;
    win.keyMap[ 14 ] = teWindowEvent::KeyCode::Backspace;
    win.keyMap[ 111 ] = teWindowEvent::KeyCode::Delete;
    win.keyMap[ 52 ] = teWindowEvent::KeyCode::Dot;
    win.keyMap[ 102 ] = teWindowEvent::KeyCode::Home;
    win.keyMap[ 107 ] = teWindowEvent::KeyCode::End;
    win.keyMap[ 12 ] = teWindowEvent::KeyCode::Plus;
    win.keyMap[ 53 ] = teWindowEvent::KeyCode::Minus;
    win.keyMap[ 86 ] = teWindowEvent::KeyCode::Less;
    win.keyMap[ 78 ] = teWindowEvent::KeyCode::NumpadPlus;
    win.keyMap[ 74 ] = teWindowEvent::KeyCode::NumpadMinus;
    win.keyMap[ 15 ] = teWindowEvent::KeyCode::Tab;

    win.keyMap[ 39 ] = teWindowEvent::KeyCode::OE;
    win.keyMap[ 40 ] = teWindowEvent::KeyCode::AE;
    win.keyMap[ 30 ] = teWindowEvent::KeyCode::A;
    win.keyMap[ 48 ] = teWindowEvent::KeyCode::B;
    win.keyMap[ 46 ] = teWindowEvent::KeyCode::C;
    win.keyMap[ 32 ] = teWindowEvent::KeyCode::D;
    win.keyMap[ 18 ] = teWindowEvent::KeyCode::E;
    win.keyMap[ 33 ] = teWindowEvent::KeyCode::F;
    win.keyMap[ 34 ] = teWindowEvent::KeyCode::G;
    win.keyMap[ 35 ] = teWindowEvent::KeyCode::H;
    win.keyMap[ 23 ] = teWindowEvent::KeyCode::I;
    win.keyMap[ 36 ] = teWindowEvent::KeyCode::J;
    win.keyMap[ 37 ] = teWindowEvent::KeyCode::K;
    win.keyMap[ 38 ] = teWindowEvent::KeyCode::L;
    win.keyMap[ 50 ] = teWindowEvent::KeyCode::M;
    win.keyMap[ 49 ] = teWindowEvent::KeyCode::N;
    win.keyMap[ 24 ] = teWindowEvent::KeyCode::O;
    win.keyMap[ 25 ] = teWindowEvent::KeyCode::P;
    win.keyMap[ 16 ] = teWindowEvent::KeyCode::Q;
    win.keyMap[ 19 ] = teWindowEvent::KeyCode::R;
    win.keyMap[ 31 ] = teWindowEvent::KeyCode::S;
    win.keyMap[ 20 ] = teWindowEvent::KeyCode::T;
    win.keyMap[ 22 ] = teWindowEvent::KeyCode::U;
    win.keyMap[ 47 ] = teWindowEvent::KeyCode::V;
    win.keyMap[ 17 ] = teWindowEvent::KeyCode::W;
    win.keyMap[ 45 ] = teWindowEvent::KeyCode::X;
    win.keyMap[ 21 ] = teWindowEvent::KeyCode::Y;
    win.keyMap[ 44 ] = teWindowEvent::KeyCode::Z;

    win.keyMap[ 2 ] = teWindowEvent::KeyCode::N1;
    win.keyMap[ 3 ] = teWindowEvent::KeyCode::N2;
    win.keyMap[ 4 ] = teWindowEvent::KeyCode::N3;
    win.keyMap[ 5 ] = teWindowEvent::KeyCode::N4;
    win.keyMap[ 6 ] = teWindowEvent::KeyCode::N5;
    win.keyMap[ 7 ] = teWindowEvent::KeyCode::N6;
    win.keyMap[ 8 ] = teWindowEvent::KeyCode::N7;
    win.keyMap[ 9 ] = teWindowEvent::KeyCode::N8;
    win.keyMap[10 ] = teWindowEvent::KeyCode::N9;
    win.keyMap[11 ] = teWindowEvent::KeyCode::N0;
}

static void InitGamePad()
{
    DIR* dir = opendir( "/dev/input" );
    dirent* result = readdir( dir );

    while (result != nullptr)
    {
        dirent& entry = *result;
        
        if ((entry.d_name[0] == 'j') && (entry.d_name[1] == 's'))
        {
            char full_device_path[ 267 ];
            snprintf( full_device_path, sizeof( full_device_path ), "%s/%s", "/dev/input", entry.d_name );
            int fd = open( full_device_path, O_RDONLY );

            if (fd < 0)
            {
                // Permissions could cause this code path.
                continue;
            }

            char name[ 128 ];
            ioctl( fd, JSIOCGNAME( 128 ), name);
            /*if (!strstr(name, "Microsoft X-Box 360 pad"))
            {
                //close(fd);
                std::cerr << "Not an xbox controller: " << std::string( name ) << std::endl;;
                continue;
                }*/

            int version;
            ioctl( fd, JSIOCGVERSION, &version );
            uint8_t axes;
            ioctl( fd, JSIOCGAXES, &axes );
            uint8_t buttons;
            ioctl( fd, JSIOCGBUTTONS, &buttons );
            win.gamePad.fd = fd;
            win.gamePad.isActive = true;
            // XBox One Controller values. Should also work for 360 Controller.
            win.gamePad.buttonA = 0;
            win.gamePad.buttonB = 1;
            win.gamePad.buttonX = 2;
            win.gamePad.buttonY = 3;
            win.gamePad.buttonStart = 7;
            win.gamePad.buttonBack = 6;
            win.gamePad.buttonLeftShoulder = 4;
            win.gamePad.buttonRightShoulder = 5;
            win.gamePad.dpadXaxis = 6;
            win.gamePad.dpadYaxis = 7;
            win.gamePad.leftThumbX = 0;
            win.gamePad.leftThumbY = 1;
            win.gamePad.rightThumbX = 3;
            win.gamePad.rightThumbY = 4;
            win.gamePad.deadZone = 7849;
            
            fcntl( fd, F_SETFL, O_NONBLOCK );
        }

        result = readdir( dir );
    }

    closedir( dir );
}

void* teCreateWindow( unsigned width, unsigned height, const char* title )
{
    InitKeyMap();
    InitGamePad();
    
    gwlDisplay = wl_display_connect( nullptr );
    
    if (!gwlDisplay)
    {
        printf( "No Wayland connection!\n" );
        return nullptr;
    }

    wl_list_init( &seats );
    wl_list_init( &outputs );

    wl_registry* wlRegistry = wl_display_get_registry( gwlDisplay );
    wl_registry_add_listener( wlRegistry, &registryListener, nullptr );
    wl_display_roundtrip( gwlDisplay );
    wl_display_roundtrip( gwlDisplay ); // Note: This is not a copy/paste bug. If it's removed, the window size will be wrong.

    Output* output;

    wl_list_for_each( output, &outputs, link )
    {
        //window->scale = MAX(window->scale, output->scale);
        printf( "output scale: %d, window scale: %d\n", output->scale, window.scale );
        width /= output->scale;
        height /= output->scale;
    }

    win.width = width;
    win.height = height;

    wl_list_init( &outputs );
    window.wlSurface = wl_compositor_create_surface( wlCompositor );
    gwlSurface = window.wlSurface;
    wl_surface_add_listener( window.wlSurface, &surfaceListener, &window );

    decorContext = libdecor_new( gwlDisplay, &libdecor_iface );
    window.frame = libdecor_decorate( decorContext, window.wlSurface, &decorFrameInterface, &window );
    libdecor_frame_set_app_id( window.frame, "theseus" );
    libdecor_frame_set_title( window.frame, "Theseus Engine" );
    libdecor_frame_map( window.frame );

    while (!window.isConfigured)
    {
        if (libdecor_dispatch( decorContext, 0 ) < 0 )
        {
            printf( "Waiting for window configuration failed!\n" );
            exit( 1 );
        }
    }

    return nullptr;
}

void tePushWindowEvents()
{
    if (!win.gamePad.isActive)
    {
        return;
    }
    
    js_event j;

    while (read( win.gamePad.fd, &j, sizeof( js_event ) ) == sizeof( js_event ))
    {
        // Don't care if init or afterwards
        j.type &= ~JS_EVENT_INIT;
            
        if (j.type == JS_EVENT_BUTTON)
        {
            if (j.number == win.gamePad.buttonA && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonA;
            }
            else if (j.number == win.gamePad.buttonB && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonB;
            }
            else if (j.number == win.gamePad.buttonX && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonX;
            }
            else if (j.number == win.gamePad.buttonY && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonY;
            }
            else if (j.number == win.gamePad.buttonStart && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonStart;
            }
            else if (j.number == win.gamePad.buttonBack && j.value > 0)
            {
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonBack;
            }
            else
            {
            }
        }
        else if (j.type == JS_EVENT_AXIS)
        {
            if (j.number == win.gamePad.leftThumbX)
            {
                const float x = ProcessGamePadStickValue( j.value, win.gamePad.deadZone );
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadLeftThumbState;
                win.events[ win.eventIndex ].gamePadThumbX = x;
                win.events[ win.eventIndex ].gamePadThumbY = win.gamePad.lastLeftThumbY;
                win.gamePad.lastLeftThumbX = x;
            }
            else if (j.number == win.gamePad.leftThumbY)
            {
                const float y = ProcessGamePadStickValue( j.value, win.gamePad.deadZone );
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadLeftThumbState;
                win.events[ win.eventIndex ].gamePadThumbX = win.gamePad.lastLeftThumbX;
                win.events[ win.eventIndex ].gamePadThumbY = -y;
                win.gamePad.lastLeftThumbY = -y;
            }
            else if (j.number == win.gamePad.rightThumbX)
            {
                const float x = ProcessGamePadStickValue( j.value, win.gamePad.deadZone );
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadRightThumbState;
                win.events[ win.eventIndex ].gamePadThumbX = x;
                win.events[ win.eventIndex ].gamePadThumbY = win.gamePad.lastRightThumbY;
                win.gamePad.lastRightThumbX = x;
            }
            else if (j.number == win.gamePad.rightThumbY)
            {
                const float y = ProcessGamePadStickValue( j.value, win.gamePad.deadZone );
                IncEventIndex();
                win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadRightThumbState;
                win.events[ win.eventIndex ].gamePadThumbX = win.gamePad.lastRightThumbX;
                win.events[ win.eventIndex ].gamePadThumbY = -y;
                win.gamePad.lastRightThumbY = -y;
            }
            else if (j.number == win.gamePad.dpadXaxis)
            {
                if (j.value > 0)
                {
                    IncEventIndex();
                    win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadRight;
                }
                if (j.value < 0)
                {
                    IncEventIndex();
                    win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadLeft;
                }
            }
            else if (j.number == win.gamePad.dpadYaxis)
            {
                if (j.value < 0)
                {
                    IncEventIndex();
                    win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadUp;
                }
                if (j.value > 0)
                {
                    IncEventIndex();
                    win.events[ win.eventIndex ].type = teWindowEvent::Type::GamePadButtonDPadDown;
                }
            }
        }
    }
}

void teWindowGetSize( unsigned& outWidth, unsigned& outHeight )
{
    outWidth = win.width;
    outHeight = win.height;
}

const teWindowEvent& tePopWindowEvent()
{
    if (win.eventIndex == -1)
    {
        win.events[ 0 ].type = teWindowEvent::Type::Empty;
        return win.events[ 0 ];
    }

    --win.eventIndex;
    return win.events[ win.eventIndex + 1 ];
}

void WaylandDispatch()
{
    int timeout = -1;
    libdecor_dispatch( decorContext, timeout );
    //printf("libdecor_dispatch res: %d\n", res);
}
