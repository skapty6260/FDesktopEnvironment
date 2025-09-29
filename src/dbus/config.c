#include <fde/dbus.h>

#define CONFIG_INTERFACE "org.fde.Compositor.Config"

static method_entry_t config_entries[] = {
    { CONFIG_INTERFACE, "GetConfigValue", handle_set_property },
    { CONFIG_INTERFACE, "SetConfigValue", handle_set_property },
    { CONFIG_INTERFACE, "ReloadConfig", handle_set_property },
};

method_entry_t *get_config_method_entries() {
    return config_entries;
}