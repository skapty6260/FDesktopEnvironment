#include <fde/plugin-system.h>
#include <string.h>

void plugin_list_add(compositor_t *server, plugin_instance_t *plugin) {
    wl_list_insert(&server->plugins, &plugin->link);
}
void plugin_list_remove(compositor_t *server, plugin_instance_t *plugin) {
    wl_list_remove(&plugin->link);
}
plugin_instance_t *plugin_list_find_by_name(compositor_t *server, const char *name) {
    plugin_instance_t *p;
    wl_list_for_each(p, &server->plugins, link) {
        if (strcmp(p->name, name) == 0) {
            return p;
        }
    }
    return NULL;
}