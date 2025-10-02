#include <fde/plugin-system.h>
#include <fde/dbus.h>
#include <fde/utils/log.h>
#include <fde/comp/compositor.h>

#include <stdlib.h>

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
    
    server->socket = wl_display_add_socket_auto(server->wl_display);
	if (!server->socket) {
		wlr_backend_destroy(server->backend);
		return 1;
	}

    // Set the WAYLAND_DISPLAY environment variable, so that clients know how to connect
    // to our server
	setenv("WAYLAND_DISPLAY", server->socket, true);

    // Set up env vars to encourage applications to use wayland if possible
    setenv("QT_QPA_PLATFORM", "wayland", true);
    setenv("MOZ_ENABLE_WAYLAND", "1", true);

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

void comp_run(compositor_t *server) {
    fde_log(FDE_INFO, "Running compositor");
	wl_display_run(server->wl_display);
}

void comp_destroy(compositor_t *server, struct fde_config *config, char *config_path) {
    if (!server) return;

    fde_log(FDE_DEBUG, "Destroying server resources");

    if (server->dbus_source) {
        wl_event_source_remove(server->dbus_source);
        server->dbus_source = NULL;
    }
    cleanup_dbus(server);

    if (server->wl_display) {
        wl_display_destroy_clients(server->wl_display);
        wl_display_destroy(server->wl_display);
        server->wl_display = NULL;
        server->wl_event_loop = NULL;  // Авто-уничтожается с display
    }

    if (config) free_config(config);
    FREE_AND_NULL(config_path);

    free(server);
    fde_log(FDE_INFO, "Server fully destroyed");
}
