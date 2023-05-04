#include <wayland-client.h>
#include "window.h"
#include "te_stdlib.h"
#include <stdio.h>

#define XDG_SURFACE_ACK_CONFIGURE 4
#define XDG_SURFACE_GET_TOPLEVEL 1
#define XDG_WM_BASE_GET_XDG_SURFACE 2
#define XDG_WM_BASE_PONG 3
#define XDG_TOPLEVEL_SET_TITLE 2

wl_display* wlDisplay;
wl_surface* wlSurface;

constexpr int EventStackSize = 100;

struct WindowImpl
{
    teWindowEvent events[ EventStackSize ];
    teWindowEvent::KeyCode keyMap[ 256 ] = {};
    int eventIndex = -1;
};

WindowImpl win;

struct client_state
{
    wl_registry* wl_registry;
    wl_shm* wl_shm;
    wl_compositor* wl_compositor;
    struct xdg_wm_base* xdg_wm_base;
    struct xdg_surface* xdg_surface;
    struct xdg_toplevel* xdg_toplevel;
};

static struct wl_buffer* draw_frame( client_state* state )
{
    return NULL;
}

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
	struct wl_proxy* id = wl_proxy_marshal_flags((struct wl_proxy*) xdg_surface,
			 XDG_SURFACE_GET_TOPLEVEL, &xdg_toplevel_interface, wl_proxy_get_version( (struct wl_proxy *) xdg_surface), 0, nullptr );

	return (struct xdg_toplevel*) id;
}

static void xdg_surface_configure( void* data, xdg_surface* xdg_surface, uint32_t serial )
{
    client_state* state = (client_state*)data;
    xdg_surface_ack_configure( xdg_surface, serial );

    wl_buffer* buffer = draw_frame( state );
    wl_surface_attach( wlSurface, buffer, 0, 0 );
    wl_surface_commit( wlSurface );
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
	void (*ping)(void *data,
		     xdg_wm_base *xdg_wm_base,
		     uint32_t serial);
};

static inline int xdg_wm_base_add_listener( xdg_wm_base* xdg_wm_base,
			 const xdg_wm_base_listener *listener, void* data )
{
	return wl_proxy_add_listener((struct wl_proxy *) xdg_wm_base,
				     (void (**)(void)) listener, data);
}

static xdg_surface* xdg_wm_base_get_xdg_surface( xdg_wm_base* xdg_wm_base, struct wl_surface* surface )
{
	wl_proxy* id = wl_proxy_marshal_flags((struct wl_proxy *) xdg_wm_base,
			 XDG_WM_BASE_GET_XDG_SURFACE, &xdg_surface_interface, wl_proxy_get_version((struct wl_proxy *) xdg_wm_base), 0, NULL, surface);

	return (struct xdg_surface *) id;
}

static xdg_wm_base_listener xdg_wm_base_listener = {};

static void registry_global( void* data, struct wl_registry* wl_registry,
        uint32_t name, const char* interface, uint32_t version )
{
    client_state* state = (client_state*)data;

    printf("registry_global %s\n", interface);
    
    if (strcmp( interface, wl_shm_interface.name ) == 0)
    {
        if (state)
        {
            state->wl_shm = (wl_shm*)wl_registry_bind( wl_registry, name, &wl_shm_interface, 1 );
        }
        else
        {
            printf( "shm state null\n" );
        }
    }
    else if (strcmp( interface, wl_compositor_interface.name ) == 0)
    {
        if (state)
        {
            state->wl_compositor = (wl_compositor*)wl_registry_bind( wl_registry, name, &wl_compositor_interface, 4 );
        }
        else
        {
            printf( "compositor state null\n" );
        }
    }
    else if (strcmp( interface, xdg_wm_base_interface.name ) == 0)
    {
        if (state)
        {
            state->xdg_wm_base = (xdg_wm_base*)wl_registry_bind( wl_registry, name, &xdg_wm_base_interface, 1 );
            xdg_wm_base_add_listener( state->xdg_wm_base, &xdg_wm_base_listener, state );
        }
        else
        {
            printf( "xdg_wm_base state null\n" );
        }
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

static wl_registry_listener wl_registry_listener = {};

client_state state = {};

void* teCreateWindow( unsigned width, unsigned height, const char* title )
{
    xdg_surface_listener.configure = xdg_surface_configure;
    xdg_wm_base_listener.ping = xdg_wm_base_ping;
    wl_registry_listener.global = registry_global;
    wl_registry_listener.global_remove = registry_global_remove;

    wlDisplay = wl_display_connect( nullptr );
    teAssert( wlDisplay );
    state.wl_registry = wl_display_get_registry( wlDisplay );
    wl_registry_add_listener( state.wl_registry, &wl_registry_listener, NULL );
    wl_display_roundtrip( wlDisplay );

    wlSurface = wl_compositor_create_surface( state.wl_compositor );

    state.xdg_surface = xdg_wm_base_get_xdg_surface( state.xdg_wm_base, wlSurface );
    xdg_surface_add_listener( state.xdg_surface, &xdg_surface_listener, &state );
    state.xdg_toplevel = xdg_surface_get_toplevel( state.xdg_surface );
    xdg_toplevel_set_title( state.xdg_toplevel, "Example client" );
    wl_surface_commit( wlSurface );

    while (wl_display_dispatch( wlDisplay ))
    {
        /* This space deliberately left blank */
    }
    return nullptr;
}

void tePushWindowEvents()
{
}

const teWindowEvent& tePopWindowEvent()
{
    return win.events[ 0 ];
}
