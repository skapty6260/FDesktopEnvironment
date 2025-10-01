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

    // CLIENT SOCKETS
	const char *socket_name;  // Имя сокета (e.g., "wayland-0"; от wl_display_add_socket_auto).
    char *runtime_dir;  // XDG_RUNTIME_DIR (copied from env).
    int socket_fd;      // WAYLAND_SOCKET fd (если nested; -1 otherwise).
} compositor_t;

// Singleton
extern compositor_t *server;

bool comp_start(compositor_t *server);
void comp_run(compositor_t *server);
bool comp_init(compositor_t *server);
void comp_destroy(compositor_t *server);