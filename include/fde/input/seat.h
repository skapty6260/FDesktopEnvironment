#pragma once

#include <fde/comp/compositor.h>

#include <wayland-util.h>

typedef struct compositor compositor_t;

typedef struct fde_seat {
    compositor_t *server;
    struct wlr_seat *wlr_seat;

    struct wl_list keyboards;

    struct wl_list server_link;
} fde_seat_t;

fde_seat_t *create_seat(compositor_t *server, char *name);