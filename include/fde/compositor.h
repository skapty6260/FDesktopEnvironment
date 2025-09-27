#pragma once

#include <dbus/dbus.h>
#include <wayland-util.h>
#include <wayland-server.h>
#include <wlr/backend.h>

typedef struct compositor {
    struct wl_display *wl_display;
    struct wlr_backend *backend;
    struct wl_event_loop *wl_event_loop;
    struct wlr_session *session;
	struct wlr_compositor *compositor;
    struct wlr_renderer *renderer;

    // IPC & Plugins
    DBusConnection *dbus_conn;
    char *dbus_service_name;
    struct wl_list plugins; // plugin_instance_t
    struct wl_event_source *dbus_source;
} compositor_t;

// Singleton
extern compositor_t *server;

bool comp_start(compositor_t *server);
void comp_run(compositor_t *server);
bool comp_init(compositor_t *server);
void comp_destroy(compositor_t *server);