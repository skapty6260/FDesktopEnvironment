#include <fde/util/log.h>
#include <fde/compositor/compositor.h>

#include <wayland-server-core.h>
#include <wlr/backend/multi.h>

bool server_init(struct fde_server *server) {
    fde_log(FDE_DEBUG, "Initializing Wayland server");
    server->wl_display = wl_display_create();
    server->wl_event_loop = wl_display_get_event_loop(server->wl_display);

    server->backend = wlr_backend_autocreate(server->wl_event_loop, &server->session);
    if (!server->backend) {
		fde_log(FDE_ERROR, "Unable to create backend");
		return false;
	}

    // wlr_multi_for_each_backend(server->backend, detect_proprietary, NULL); // TODO: method detect proprietary
    
    server->renderer = wlr_renderer_autocreate(server->backend);
	if (!server->renderer) {
		fde_log(FDE_ERROR, "Failed to create renderer");
		return false;
	}

    server->allocator = wlr_allocator_autocreate(server->backend, server->renderer);
	if (!server->allocator) {
		fde_log(FDE_ERROR, "Failed to create allocator");
		return false;
	}

    server->compositor = wlr_compositor_create(server->wl_display, 6, server->renderer);
	wlr_subcompositor_create(server->wl_display);

    server->data_device_manager = wlr_data_device_manager_create(server->wl_display);

    return true;
}

bool server_start(struct fde_server *server) {
	fde_log(FDE_INFO, "Starting backend on wayland display '%s'",
			server->socket);
	if (!wlr_backend_start(server->backend)) {
		fde_log(FDE_ERROR, "Failed to start backend");
		wlr_backend_destroy(server->backend);
		return false;
	}

	return true;
}

void server_run(struct fde_server *server) {
	fde_log(FDE_INFO, "Running compositor on wayland display '%s'",
			server->socket);
	wl_display_run(server->wl_display);
}