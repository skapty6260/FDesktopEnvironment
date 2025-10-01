#include <fde/utils/log.h>
#include <fde/comp/compositor.h>
#include <fde/comp/socket.h>

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

    // Получить XDG_RUNTIME_DIR.
    server->runtime_dir = get_xdg_runtime_dir();
    if (!server->runtime_dir) {
        fde_log(FDE_ERROR, "Failed to get XDG_RUNTIME_DIR");
        return false;
    }
    // Обработать WAYLAND_SOCKET (nested fallback).
    if (!handle_wayland_socket_env(server)) {
        free(server->runtime_dir);
        return false;
    }
    // Создать сокет (если не nested).
    if (server->socket_fd == -1) {
        // Авто-создание: wl_display_add_socket_auto возвращает имя (e.g., "wayland-0").
        server->socket_name = wl_display_add_socket_auto(server->wl_display);
        if (!server->socket_name) {
            fde_log(FDE_ERROR, "Failed wl_display_add_socket_auto");
            free(server->runtime_dir);
            return false;
        }
        // Полный путь для логов (не env).
        char *full_path = make_socket_path(server->runtime_dir, server->socket_name);
        if (full_path) {
            fde_log(FDE_INFO, "Created socket at %s (name: %s)", full_path, server->socket_name);
            free(full_path);
        } else {
            fde_log(FDE_INFO, "Created socket name: %s (in %s)", server->socket_name, server->runtime_dir);
        }
        // Установить WAYLAND_DISPLAY = имя (libwayland добавит XDG_RUNTIME_DIR).
        setenv("WAYLAND_DISPLAY", server->socket_name, true);
    } else {
        // Nested: WAYLAND_DISPLAY обычно от parent (e.g., "wayland-1").
        const char *display_env = getenv("WAYLAND_DISPLAY");
        if (display_env) {
            server->socket_name = strdup(display_env);  // Copy имя.
            fde_log(FDE_INFO, "Nested: Using WAYLAND_DISPLAY=%s (fd %d)", display_env, server->socket_fd);
        } else {
            // Fallback: "wayland-0".
            server->socket_name = strdup("wayland-0");
            setenv("WAYLAND_DISPLAY", server->socket_name, true);
            fde_log(FDE_INFO, "Nested fallback: Set WAYLAND_DISPLAY=wayland-0 (fd %d)", server->socket_fd);
        }
    }

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
    fde_log(FDE_INFO, "Running compositor on wayland display '%s'",
			server->socket_name);
	wl_display_run(server->wl_display);
    comp_destroy(server);
}

void comp_destroy(compositor_t *server) {
    if (!server) return;

    fde_log(FDE_DEBUG, "Destroying server resources");

    wl_display_destroy_clients(server->wl_display);

    if (server->socket_name) {
        // free(server->socket_name);
        server->socket_name = NULL;
    }

    if (server->compositor) {
        server->compositor = NULL;
    }
    // Subcompositor is auto-destroyed with display.

    // Destroy backend (outputs, inputs; after listeners).
    if (server->backend) {
        wlr_backend_destroy(server->backend);
        server->backend = NULL;
    }

    // Destroy display last (handles event loop, socket).
    if (server->wl_display) {
        wl_display_destroy(server->wl_display);
        server->wl_display = NULL;
    }

    // Clear session if needed (wlroots handles).
    server->session = NULL;

    // Free server struct if owned (e.g., calloc in main); otherwise, just log.
    fde_log(FDE_INFO, "Server destroyed");
}