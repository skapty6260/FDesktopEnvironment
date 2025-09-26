#pragma once

#include <fde/compositor.h>

#include <stdbool.h>

#include <wayland-util.h>

typedef enum {
    PLUGIN_STATE_LOADED,
    PLUGIN_STATE_UNLOADING,
    PLUGIN_STATE_ERROR
} plugin_state_t;

typedef struct {
    struct wl_list link;  // wl_list in compositor_t.plugins.
    char *path;
    void *handle;  // dlopen.
    plugin_state_t state;
    int refcount;  // Для safe unload (если used).

    // Callbacks (dlsym).
    bool (*init)(compositor_t *comp, void **plugin_data);  // Return private data.
    void (*update)(compositor_t *comp, void *plugin_data);  // Для hot-reload.
    void (*destroy)(compositor_t *comp, void *plugin_data);

    void *private_data;  // State плагина (e.g., scene nodes; сохраняется при reload).
} plugin_t;

plugin_t *find_plugin_by_path(compositor_t *comp, const char *path);

// API functions (public для core).
bool load_plugin(compositor_t *comp, const char *path, void **out_data);  // Load single.
bool unload_plugin(compositor_t *comp, plugin_t *p);  // Safe unload.
void scan_and_load_plugins(compositor_t *comp, const char *dir); // Scan directory for .so