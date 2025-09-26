#pragma once

#include <wayland-util.h>

typedef struct compositor {
    struct wl_display *wl_display;
    struct wlr_backend *backend;

    struct wl_list plugins;

    struct wlr_seat *seat;
} compositor_t;

// Singleton
extern compositor_t *server;