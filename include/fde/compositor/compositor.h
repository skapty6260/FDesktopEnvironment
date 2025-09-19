#pragma once

#include <wayland-server.h>

#include <wlr/backend.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/render/allocator.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_data_device.h>

#include <stdbool.h>
#include <stddef.h>

struct fde_server {
	struct wl_display *wl_display;
	struct wl_event_loop *wl_event_loop;
	struct wlr_backend *backend;
	struct wlr_session *session;
	struct wlr_renderer *renderer;
	struct wlr_allocator *allocator;
	struct wlr_compositor *compositor;

	// Display socket name
	char *socket;

	struct wlr_data_device_manager *data_device_manager;
};

extern struct fde_server server;

struct fde_debug {
    bool noatomic;         // Ignore atomic layout updates
	bool txn_timings;      // Log verbose messages about transactions
	bool txn_wait;         // Always wait for the timeout before applying
};

extern struct fde_debug debug;

bool server_start(struct fde_server *server);
void server_run(struct fde_server *server);
bool server_init(struct fde_server *server);