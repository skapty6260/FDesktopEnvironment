#pragma once

#include <fde/config.h>
#include <fde/comp/compositor.h>

#include <stdbool.h>
#include <unistd.h>

#include <dbus/dbus.h>
#include <wayland-util.h>

typedef struct plugin_instance {
    pid_t pid;
    char *name;
    char *dbus_path;
    
    struct wl_list link;

    // Metadata
    bool supports_input;
    bool supports_rendering;
    bool supports_protocols;
} plugin_instance_t;

// Функции для работы со списком (опционально, можно inline)
void plugin_list_add(compositor_t *server, plugin_instance_t *plugin);
void plugin_list_remove(compositor_t *server, plugin_instance_t *plugin);

plugin_instance_t *plugin_list_find_by_name(compositor_t *server, const char *name);
void plugin_instance_destroy(plugin_instance_t *plugin);

bool load_plugins_from_dir(compositor_t *server, struct fde_config *config);