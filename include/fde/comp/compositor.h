#pragma once

#include <fde/config.h>
#include <dbus/dbus.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wayland-server.h>
#include <wlr/backend.h>

#define DESTROY_AND_NULL(ptr, destroy_fn) do { \
    if (ptr) { \
        destroy_fn(ptr); \
        ptr = NULL; \
    } \
} while (0)

#define FREE_AND_NULL(ptr) DESTROY_AND_NULL(ptr, free)
#define WL_LIST_DESTROY(list_head, item_type, destroy_fn) do { \
    item_type *item, *tmp; \
    wl_list_for_each_safe(item, tmp, list_head, link) { \
        wl_list_remove(&item->link); \
        if (destroy_fn) destroy_fn(item); \
        else free(item); \
    } \
    wl_list_init(list_head); \
} while (0)

struct fde_config;

typedef struct compositor {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wl_event_loop *wl_event_loop;
    struct wlr_session *session;
	struct wlr_compositor *compositor;
    struct wlr_renderer *renderer;
    struct wlr_allocator *allocator;

    // IPC & Plugins
    DBusConnection *dbus_conn;
    char *dbus_service_name;
    struct wl_list plugins; // plugin_instance_t
    struct wl_event_source *dbus_source;

    const char *socket;

    struct wlr_scene *scene;

    // Output
    struct wlr_output_layout *output_layout;
    struct wl_listener new_output;
    struct wl_list outputs;
    struct wl_list workspaces;
} compositor_t;

// Singleton
extern compositor_t *server;

bool comp_start(compositor_t *server);
void comp_run(compositor_t *server);
bool comp_init(compositor_t *server);
void comp_destroy(compositor_t *server, struct fde_config *config, char *config_path);