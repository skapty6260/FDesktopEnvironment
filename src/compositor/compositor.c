#include <fde/plugin-system.h>
#include <fde/dbus.h>
#include <fde/utils/log.h>
#include <fde/comp/compositor.h>
#include <fde/config.h>
#include <fde/comp/output.h>

#include <stdlib.h>

#include <wayland-server.h>
#include <wayland-util.h>
#include <wlr/render/allocator.h>
#include <wlr/render/wlr_renderer.h>
#include <wlr/types/wlr_compositor.h>
#include <wlr/types/wlr_subcompositor.h>
#include <wlr/types/wlr_output.h>
#include <wlr/types/wlr_xdg_output_v1.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_data_device.h>
#include <wlr/types/wlr_output_power_management_v1.h>
#include <wlr/types/wlr_screencopy_v1.h>
#include <wlr/types/wlr_primary_selection_v1.h>
#include <wlr/types/wlr_scene.h>

#define CREATE_ASSIGN_N_CHECK(assign_to, create_fn, msg) \
    assign_to = create_fn; \
    if (!assign_to) {\
        fde_log(FDE_ERROR, msg); \
        return false; \
    };

bool comp_init(compositor_t *server) {
    fde_log(FDE_DEBUG, "Initializing wayland server");

    // Dynamically create workspaces according to the user configuration
    init_workspaces(&server->workspaces, config->workspaces.list, server);

    server->wl_display = wl_display_create();
    server->wl_event_loop = wl_display_get_event_loop(server->wl_display);

    CREATE_ASSIGN_N_CHECK(server->backend, wlr_backend_autocreate(server->wl_event_loop, &server->session), "Unable to create backend");
    CREATE_ASSIGN_N_CHECK(server->renderer, wlr_renderer_autocreate(server->backend), "Failed to create renderer");
    CREATE_ASSIGN_N_CHECK(server->allocator, wlr_allocator_autocreate(server->backend, server->renderer), "Failed to create allocator");
    CREATE_ASSIGN_N_CHECK(server->compositor, wlr_compositor_create(server->wl_display, 6, server->renderer), "Failed to create compositor");
	
    wlr_subcompositor_create(server->wl_display);
    wlr_data_device_manager_create(server->wl_display);
    wlr_screencopy_manager_v1_create(server->wl_display);
    wlr_primary_selection_v1_device_manager_create(server->wl_display);

    // Create an output layout, for handling the arrangement of multiple outputs
	server->output_layout = wlr_output_layout_create(server->wl_display);

    wl_list_init(&server->outputs);
    server->new_output.notify = server_new_output;
    wl_signal_add(&server->backend->events.new_output, &server->new_output);

    server->scene = wlr_scene_create();

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
