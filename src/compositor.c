#include <fde/utils/log.h>
#include <fde/compositor.h>

#include <wayland-server.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>

bool comp_init(compositor_t *server) {
    fde_log(FDE_DEBUG, "Initializing wayland server");
    server->wl_display = wl_display_create();
    server->wl_event_loop = wl_display_get_event_loop(server->wl_display);

    server->backend = wlr_backend_autocreate(server->wl_event_loop, &server->session);
    if (!server->backend) {
		fde_log(FDE_ERROR, "Unable to create backend");
		return false;
	}

    server->renderer = wlr_renderer_autocreate(server->backend);
    if (!server->renderer) {
        fde_log(FDE_ERROR, "Failed to create custom renderer");
        return false;
    }

    server->compositor = wlr_compositor_create(server->wl_display, 6, server->renderer);
    if (!server->compositor) {
        fde_log(FDE_ERROR, "Failed to create compositor");
        return false;
    }
	wlr_subcompositor_create(server->wl_display);

    return true;
}

bool comp_start(compositor_t *server) {
    fde_log(FDE_INFO, "Starting backend on wayland display '%s'", "wayland-0"); // server->socket_name
    if (!wlr_backend_start(server->backend)) {
        fde_log(FDE_ERROR, "Failed to start wayland backend.");
        wlr_backend_destroy(server->backend);
        return false;
    }

    // Startup functional (Autostart applications, etc.)

    return true;
}