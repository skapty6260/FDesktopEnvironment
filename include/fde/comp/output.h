#pragma once

#include <wayland-server.h>
#include <wayland-util.h>

#include <fde/comp/compositor.h>
#include <fde/comp/workspace.h>

typedef struct fde_workspace workspace_t;
typedef struct compositor compositor_t;

typedef struct fde_output {
    struct wl_list link;
    struct wlr_output *wlr_output;
    compositor_t *server;

    struct wl_listener frame;
	struct wl_listener request_state;
	struct wl_listener destroy;

    struct wlr_scene_output *scene_output;

    workspace_t *active_ws;
} fde_output_t;

void server_new_output(struct wl_listener *listener, void *data);