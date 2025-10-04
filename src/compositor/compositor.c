#include <fde/input/seat.h>
#include <fde/plugin-system.h>
#include <fde/dbus.h>
#include <fde/utils/log.h>
#include <fde/comp/compositor.h>
#include <fde/config.h>
#include <fde/comp/output.h>
#include <fde/input/input-manager.h>
#include <fde/input/cursor.h>

#include <stdlib.h>

#include <wayland-server-core.h>
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
#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>

#define DEFAULT_SEAT_NAME "seat0"

#define ADD_EVENT(event_name, handler, server) \
server->event_name.notify = handler; \
wl_signal_add(&server->backend->events.event_name, &server->event_name);

#define ADD_CURSOR_EVENT(evt, server_evt, handler, server) \
server->default_seat->server_evt.notify = handler; \
wl_signal_add(&server->default_seat->cursor->events.evt, &server->default_seat->server_evt);

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
    ADD_EVENT(new_output, server_new_output, server);

    server->scene = wlr_scene_create();

    ADD_EVENT(new_input, server_new_input, server);

    wl_list_init(&server->seats);
    server->default_seat = create_seat(server, DEFAULT_SEAT_NAME);

    // Cursor
    server->default_seat->cursor = wlr_cursor_create();
    wlr_cursor_attach_output_layout(server->default_seat->cursor, server->output_layout);
    server->default_seat->cursor_mgr = wlr_xcursor_manager_create(NULL, 24);

    ADD_CURSOR_EVENT(axis, cursor_axis, cursor_axis_handler, server);
    ADD_CURSOR_EVENT(motion, cursor_motion, cursor_motion_handler, server);
    ADD_CURSOR_EVENT(motion_absolute, cursor_motion_absolute, cursor_motion_absolute_handler, server);
    ADD_CURSOR_EVENT(button, cursor_button, cursor_button_handler, server);
    ADD_CURSOR_EVENT(frame, cursor_frame, cursor_frame_handler, server);

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
	if (fork() == 0) {
		execl("/bin/sh", "/bin/sh", "-c", "kitty", (void *)NULL);
	}

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
