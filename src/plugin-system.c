#define _POSIX_C_SOURCE 200809L

#include <dlfcn.h>
#include <dirent.h>  // Для scan dirs.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glob.h>    // для *.so.
#include <unistd.h>

#include <fde/compositor.h>
#include <fde/plugin-system.h>

// TODO: rework and test on dummy plugin.

bool load_plugin(compositor_t *comp, const char *path, void **out_data) {
    if (!path || access(path, R_OK) != 0) {
        fprintf(stderr, "Plugin path invalid: %s\n", path);
        return false;
    }

    // Check if already loaded (by path).
    plugin_t *existing = find_plugin_by_path(comp, path);  // Custom func: wl_list_for_each.
    if (existing && existing->state == PLUGIN_STATE_LOADED) {
        existing->refcount++;
        if (out_data) *out_data = existing->private_data;
        return true;
    }

    plugin_t *p = calloc(1, sizeof(plugin_t));
    if (!p) return false;
    p->path = strdup(path);
    p->handle = dlopen(path, RTLD_LAZY | RTLD_LOCAL);
    if (!p->handle) {
        fprintf(stderr, "dlopen failed: %s (%s)\n", path, dlerror());
        free(p); return false;
    }

    // dlsym callbacks.
    p->init = dlsym(p->handle, "plugin_init");
    p->update = dlsym(p->handle, "plugin_update");  // Опционально для reload.
    p->destroy = dlsym(p->handle, "plugin_destroy");
    if (!p->init) {
        fprintf(stderr, "No plugin_init in %s\n", path);
        dlclose(p->handle); free(p); return false;
    }

    // Call init (pass comp; plugin registers via comp_api).
    bool init_ok = p->init(comp, &p->private_data);
    if (!init_ok) {
        fprintf(stderr, "Plugin init failed: %s\n", path);
        dlclose(p->handle); free(p); return false;
    }

    p->state = PLUGIN_STATE_LOADED;
    p->refcount = 1;
    wl_list_insert(&comp->plugins, &p->link);

    // Set comp_api if needed (global or per-plugin).
    if (out_data) *out_data = p->private_data;

    printf("Loaded plugin: %s (data: %p)\n", path, p->private_data);
    return true;
}

bool unload_plugin(compositor_t *comp, plugin_t *p) {
    if (!p || p->refcount > 0) return false;  // Busy.

    p->state = PLUGIN_STATE_UNLOADING;
    if (p->destroy) p->destroy(comp, p->private_data);

    // Cleanup resources (e.g., remove listeners, scene nodes).
    // Custom: plugin_cleanup_hooks(p);  // Remove from render_hooks etc.

    wl_list_remove(&p->link);
    dlclose(p->handle);
    free(p->path);
    free(p->private_data);  // Если owned.
    free(p);

    printf("Unloaded plugin\n");
    return true;
}

// Scan dirs for new .so (e.g., *.so).
void scan_and_load_plugins(compositor_t *comp, const char *dir) {
    glob_t glob_res;
    char pattern[512];
    snprintf(pattern, sizeof(pattern), "%s/*.so", dir);

    glob(pattern, (1 << 12), NULL, &glob_res);
    for (size_t j = 0; j < glob_res.gl_pathc; ++j) {
        const char *path = glob_res.gl_pathv[j];
        // Skip if already loaded.
        if (!find_plugin_by_path(comp, path)) {
            load_plugin(comp, path, NULL);
        }
    }
    globfree(&glob_res);
}

// Helper: find_plugin_by_path (wl_list_for_each_safe).
plugin_t *find_plugin_by_path(compositor_t *comp, const char *path) {
    plugin_t *p;
    wl_list_for_each(p, &comp->plugins, link) {
        if (strcmp(p->path, path) == 0) return p;
    }
    return NULL;
}