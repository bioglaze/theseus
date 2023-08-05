#include <wayland-client.h>
#include <linux/input.h>
#include "window.h"
#include "te_stdlib.h"
#include <stdio.h>
#include <time.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <libdecor.h>

#define XDG_SURFACE_ACK_CONFIGURE 4
#define XDG_SURFACE_GET_TOPLEVEL 1
#define XDG_WM_BASE_GET_XDG_SURFACE 2
#define XDG_WM_BASE_PONG 3
#define XDG_TOPLEVEL_SET_TITLE 2

wl_display* wlDisplay;
wl_surface* wlSurface;

// TODO: move into client_state if possible
wl_seat* seat;
wl_surface* cursor_surface;

constexpr int EventStackSize = 100;

struct WindowImpl
{
    teWindowEvent events[ EventStackSize ];
    teWindowEvent::KeyCode keyMap[ 256 ] = {};
    int eventIndex = -1;
    unsigned width = 0;
    unsigned height = 0;

    // FIXME: this is a hack to get mouse coordinate into mouse move event.
    //       A proper fix would be to read mouse coordinate when mouse move event is detected.
    unsigned lastMouseX = 0;
    unsigned lastMouseY = 0;
};

WindowImpl win;

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
    win.keyMap[ 1 ] = teWindowEvent::KeyCode::Escape;
    win.keyMap[ 57 ] = teWindowEvent::KeyCode::Space;

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
}

void IncEventIndex()
{
    if (win.eventIndex < EventStackSize - 1)
    {
        ++win.eventIndex;
    }
}

static wl_buffer_listener wlBufferListener = {};

struct client_state
{
    struct wl_registry* wl_registry;
    struct wl_shm* wl_shm;
    struct wl_compositor* wl_compositor;
    struct xdg_wm_base* xdg_wm_base;
    struct xdg_surface* xdg_surface;
    struct xdg_toplevel* xdg_toplevel;
};

extern const struct wl_interface wl_output_interface;
extern const struct wl_interface wl_seat_interface;
extern const struct wl_interface wl_surface_interface;
extern const struct wl_interface xdg_popup_interface;
extern const struct wl_interface xdg_positioner_interface;
extern const struct wl_interface xdg_surface_interface;
extern const struct wl_interface xdg_toplevel_interface;

static const struct wl_interface *xdg_shell_types[] = {
	NULL,
	NULL,
	NULL,
	NULL,
	&xdg_positioner_interface,
	&xdg_surface_interface,
	&wl_surface_interface,
	&xdg_toplevel_interface,
	&xdg_popup_interface,
	&xdg_surface_interface,
	&xdg_positioner_interface,
	&xdg_toplevel_interface,
	&wl_seat_interface,
	NULL,
	NULL,
	NULL,
	&wl_seat_interface,
	NULL,
	&wl_seat_interface,
	NULL,
	NULL,
	&wl_output_interface,
	&wl_seat_interface,
	NULL,
	&xdg_positioner_interface,
	NULL,
};

static const struct wl_message xdg_wm_base_requests[] = {
	{ "destroy", "", xdg_shell_types + 0 },
	{ "create_positioner", "n", xdg_shell_types + 4 },
	{ "get_xdg_surface", "no", xdg_shell_types + 5 },
	{ "pong", "u", xdg_shell_types + 0 },
};

static const struct wl_message xdg_wm_base_events[] = {
	{ "ping", "u", xdg_shell_types + 0 },
};

const struct wl_interface xdg_wm_base_interface = {
	"xdg_wm_base", 4,
	4, xdg_wm_base_requests,
	1, xdg_wm_base_events,
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

struct xdg_surface_listener {
	void (*configure)(void *data,
			  struct xdg_surface *xdg_surface,
			  uint32_t serial);
};

static void xdg_wm_base_pong( xdg_wm_base* xdg_wm_base, uint32_t serial )
{
	wl_proxy_marshal_flags( (struct wl_proxy *) xdg_wm_base,
			 XDG_WM_BASE_PONG, NULL, wl_proxy_get_version( (struct wl_proxy *) xdg_wm_base), 0, serial );
}

static void xdg_surface_ack_configure( xdg_surface* xdg_surface, uint32_t serial )
{
	wl_proxy_marshal_flags( (struct wl_proxy*) xdg_surface,
			 XDG_SURFACE_ACK_CONFIGURE, NULL, wl_proxy_get_version( (struct wl_proxy*) xdg_surface), 0, serial );
}

static xdg_toplevel* xdg_surface_get_toplevel( xdg_surface* xdg_surface )
{
	struct wl_proxy* id = wl_proxy_marshal_flags( (struct wl_proxy*) xdg_surface,
			 XDG_SURFACE_GET_TOPLEVEL, &xdg_toplevel_interface, wl_proxy_get_version( (struct wl_proxy *) xdg_surface), 0, nullptr );

	return (struct xdg_toplevel*) id;
}

static void xdg_surface_configure( void* data, xdg_surface* xdg_surface, uint32_t serial )
{
}

static xdg_surface_listener xdg_surface_listener = {};

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

static void registry_global_remove( void* data, wl_registry* wl_registry, uint32_t name )
{
    /* This space deliberately left blank */
}

static void xdg_wm_base_ping( void* data, xdg_wm_base* xdg_wm_base, uint32_t serial )
{
    xdg_wm_base_pong( xdg_wm_base, serial );
}

struct xdg_wm_base_listener
{
	void (*ping)( void* data, xdg_wm_base* xdg_wm_base, uint32_t serial );
};

static void pointer_enter( void* data, wl_pointer* pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y )
{
}

static void pointer_leave( void* data, wl_pointer* pointer, uint32_t serial, struct wl_surface* surface )
{
}

static void pointer_button( void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t pointerState )
{
    IncEventIndex();

    if (button == BTN_LEFT)
    {
        win.events[ win.eventIndex ].type = pointerState == WL_POINTER_BUTTON_STATE_PRESSED ? teWindowEvent::Type::Mouse1Down : teWindowEvent::Type::Mouse1Up;
    }
    else if (button == BTN_RIGHT)
    {
        win.events[ win.eventIndex ].type = pointerState == WL_POINTER_BUTTON_STATE_PRESSED ? teWindowEvent::Type::Mouse2Down : teWindowEvent::Type::Mouse2Up;
    }
    
    win.events[ win.eventIndex ].x = win.lastMouseX;
    win.events[ win.eventIndex ].y = win.lastMouseY;
}

static void pointer_axis( void* data, wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value )
{
}

static void pointer_motion( void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y )
{
    IncEventIndex();
    win.events[ win.eventIndex ].type = teWindowEvent::Type::MouseMove;
    win.events[ win.eventIndex ].x = (float)wl_fixed_to_double( x );
    win.events[ win.eventIndex ].y = win.height - (float)wl_fixed_to_double( y );
    
    win.lastMouseX = win.events[ win.eventIndex ].x;
    win.lastMouseY = win.events[ win.eventIndex ].y;
}

wl_pointer_listener pointer_listener = { &pointer_enter, &pointer_leave, &pointer_motion, &pointer_button, &pointer_axis, nullptr, nullptr, nullptr, nullptr, nullptr };

client_state state = {};

void keyMap( void* data, wl_keyboard* wl_keyboard, uint format, int fd, uint size )
{
    printf("keymap\n");
}

void keyEnter( void* data, wl_keyboard* wl_keyboard, uint serial, wl_surface* surface, wl_array* keys )
{
    printf("keyEnter\n");
}

void keyLeave( void* data, wl_keyboard* wl_keyboard, uint serial, wl_surface* surface )
{
    printf("keyLeave\n");
}

void keyKey( void* data, wl_keyboard* wl_keyboard, uint serial, uint time, uint key, uint keyState )
{
    if (keyState == 1)
        printf("keyKey %u state %u\n", key, keyState);

    IncEventIndex();
    win.events[ win.eventIndex ].type = keyState == 1 ? teWindowEvent::Type::KeyDown : teWindowEvent::Type::KeyUp;
    win.events[ win.eventIndex ].keyCode = win.keyMap[ key ];
}

void keyModifiers( void* data, wl_keyboard* wl_keyboard, uint serial, uint mods_depressed, uint mods_latched, uint mods_locked, uint group )
{
    printf("keyModifiers\n");
}

void keyRepeat( void* data, wl_keyboard* wl_keyboard, int rate, int delay )
{
    printf("keyRepeat\n");
}

wl_keyboard_listener keyboard_listener = { &keyMap, &keyEnter, &keyLeave, &keyKey, &keyModifiers, &keyRepeat };

static void seat_capabilities( void* data, wl_seat* theSeat, uint32_t capabilities )
{
    if (capabilities & WL_SEAT_CAPABILITY_POINTER)
    {
        wl_pointer* pointer = wl_seat_get_pointer( theSeat );
        wl_pointer_add_listener( pointer, &pointer_listener, data );
        cursor_surface = wl_compositor_create_surface( state.wl_compositor );
    }

    if (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)
    {
        wl_keyboard* keyboard = wl_seat_get_keyboard( seat );
        wl_keyboard_add_listener( keyboard, &keyboard_listener, nullptr );
    }
}

static wl_seat_listener seat_listener = { &seat_capabilities, nullptr };

static inline int xdg_wm_base_add_listener( xdg_wm_base* xdg_wm_base, const xdg_wm_base_listener* listener, void* data )
{
	return wl_proxy_add_listener( (struct wl_proxy*) xdg_wm_base,
				     (void (**)(void)) listener, data );
}

static xdg_surface* xdg_wm_base_get_xdg_surface( xdg_wm_base* xdg_wm_base, struct wl_surface* surface )
{
	wl_proxy* id = wl_proxy_marshal_flags((struct wl_proxy *) xdg_wm_base,
			 XDG_WM_BASE_GET_XDG_SURFACE, &xdg_surface_interface, wl_proxy_get_version( (struct wl_proxy *) xdg_wm_base), 0, nullptr, surface );

	return (struct xdg_surface *) id;
}

static xdg_wm_base_listener xdg_wm_base_listener = {};

static void registry_global( void* data, struct wl_registry* wl_registry,
        uint32_t name, const char* interface, uint32_t version )
{
    client_state* theState = (client_state*)data;

    if (strcmp( interface, wl_shm_interface.name ) == 0)
    {
        theState->wl_shm = (wl_shm*)wl_registry_bind( wl_registry, name, &wl_shm_interface, 1 );
    }
    else if (strcmp( interface, wl_compositor_interface.name ) == 0)
    {
        theState->wl_compositor = (wl_compositor*)wl_registry_bind( wl_registry, name, &wl_compositor_interface, 4 );
    }
    else if (strcmp( interface, xdg_wm_base_interface.name ) == 0)
    {
        theState->xdg_wm_base = (xdg_wm_base*)wl_registry_bind( wl_registry, name, &xdg_wm_base_interface, 1 );
        xdg_wm_base_add_listener( theState->xdg_wm_base, &xdg_wm_base_listener, theState );
    }
    else if (strcmp( interface, "wl_seat" ) == 0)
    {
        seat = static_cast< wl_seat* >(wl_registry_bind( wl_registry, name, &wl_seat_interface, 1 ) );
        wl_seat_add_listener( seat, &seat_listener, data );
    }
    else if (strcmp( interface, "wl_output" ) == 0)
    {
        printf( "registry_global: wl_output\n" );
    }
}

static int xdg_surface_add_listener( xdg_surface* xdg_surface,
			 const struct xdg_surface_listener* listener, void* data )
{
	return wl_proxy_add_listener((struct wl_proxy *) xdg_surface,
				     (void (**)(void)) listener, data);
}

static inline void xdg_toplevel_set_title( xdg_toplevel* xdg_toplevel, const char* title )
{
	wl_proxy_marshal_flags((struct wl_proxy *) xdg_toplevel,
			 XDG_TOPLEVEL_SET_TITLE, NULL, wl_proxy_get_version((struct wl_proxy *) xdg_toplevel), 0, title);
}

static void wl_buffer_release( void* data, wl_buffer* wl_buffer )
{
    wl_buffer_destroy( wl_buffer );
}

static wl_registry_listener wl_registry_listener = {};

void handle_error( libdecor* context, enum libdecor_error error, const char* message )
{
    printf( "libdecor error: %d: %s\n", error, message );
	teAssert( !"libdecor error" );
}

static struct libdecor_interface libdecor_iface =
{
	handle_error,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr
};

libdecor* decorContext;
libdecor_frame_interface decorFrameInterface;
libdecor_window_state decorWindowState;

static void handle_configure( libdecor_frame* frame, libdecor_configuration* configuration, void* user_data )
{
    printf("handle_configure\n");
    libdecor_window_state window_state = LIBDECOR_WINDOW_STATE_NONE;
    
    if (!libdecor_configuration_get_window_state( configuration, &window_state ) )
    {
		window_state = LIBDECOR_WINDOW_STATE_NONE;
    }

    int width, height;
    
    if (!libdecor_configuration_get_content_size( configuration, frame, &width, &height ))
    {
		//width = window->content_width;
		//height = window->content_height;
	}

    printf( "handle_configure: width: %d, height %d\n", width, height );

    libdecor_state* decState = libdecor_state_new( width, height );
	libdecor_frame_commit( frame, decState, configuration );
	libdecor_state_free( decState );

	/*if (libdecor_frame_is_floating( window->frame ))
    {
		window->floating_width = width;
		window->floating_height = height;
	}

    redraw(window);
    */
}

static void handle_close( libdecor_frame* frame, void* user_data )
{
    exit(EXIT_SUCCESS);
}

void commit()
{
    wl_surface_commit( wlSurface );
}

static void handle_commit( libdecor_frame* frame, void* user_data )
{
    printf("handle_commit\n");
}

static void handle_dismiss_popup( libdecor_frame* frame, const char* seat_name, void* user_data )
{
}

void* teCreateWindow( unsigned width, unsigned height, const char* title )
{
    InitKeyMap();
    
    win.width = width;
    win.height = height;
    
    xdg_surface_listener.configure = xdg_surface_configure;
    xdg_wm_base_listener.ping = xdg_wm_base_ping;
    wl_registry_listener.global = registry_global;
    wl_registry_listener.global_remove = registry_global_remove;
    wlBufferListener.release = wl_buffer_release;

    wlDisplay = wl_display_connect( nullptr );
    teAssert( wlDisplay );
    state.wl_registry = wl_display_get_registry( wlDisplay );
    wl_registry_add_listener( state.wl_registry, &wl_registry_listener, &state );
    wl_display_roundtrip( wlDisplay );

    wlSurface = wl_compositor_create_surface( state.wl_compositor );

    state.xdg_surface = xdg_wm_base_get_xdg_surface( state.xdg_wm_base, wlSurface );
    xdg_surface_add_listener( state.xdg_surface, &xdg_surface_listener, &state );
    state.xdg_toplevel = xdg_surface_get_toplevel( state.xdg_surface );
    xdg_toplevel_set_title( state.xdg_toplevel, "Example client" );
    wl_surface_commit( wlSurface );

    /*decorContext = libdecor_new( wlDisplay, &libdecor_iface );

    decorFrameInterface.configure = handle_configure;
    decorFrameInterface.close = handle_close;
    decorFrameInterface.commit = handle_commit;
    decorFrameInterface.dismiss_popup = handle_dismiss_popup;

    libdecor_frame* decorFrame = libdecor_decorate( decorContext, wlSurface, &decorFrameInterface, nullptr );
    libdecor_frame_set_app_id( decorFrame, "Theseus Engine");
    libdecor_frame_set_title( decorFrame, title );
    libdecor_frame_map( decorFrame );*/

    return nullptr;
}

void WaylandDispatch()
{
    int timeout = -1;
    printf("libdecor_dispatch before\n");
    //int res = libdecor_dispatch( decorContext, timeout );
    //printf("libdecor_dispath res: %d\n", res);
    wl_display_dispatch( wlDisplay );
}

void tePushWindowEvents()
{
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
