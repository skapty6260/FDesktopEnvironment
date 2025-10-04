#pragma once

#include <fde/comp/compositor.h>

#include <wayland-server-core.h>
#include <wayland-util.h>

typedef struct compositor compositor_t;

typedef struct fde_seat {
    compositor_t *server;
    struct wlr_seat *wlr_seat;

    struct wl_list keyboards;

    struct wl_list server_link;

    struct wlr_cursor *cursor;
    struct wlr_xcursor_manager *cursor_mgr;

    struct wl_listener cursor_button;
    struct wl_listener cursor_motion_absolute;
    struct wl_listener cursor_motion;
    struct wl_listener cursor_axis;
    struct wl_listener cursor_frame;
} fde_seat_t;

fde_seat_t *create_seat(compositor_t *server, char *name);