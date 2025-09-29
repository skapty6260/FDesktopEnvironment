#include <stdlib.h>

#include <fde/dbus.h>
#include <fde/utils/log.h>
#include <fde/compositor.h>
#include <fde/plugin-system.h>

#define PLUGINS_INTERFACE "org.fde.Compositor.Plugins"

static method_entry_t plugins_entries[] = {
    { "org.fde.Compositor.Plugins", "RegisterPlugin", handle_register_plugin },
    // { "org.fde.Compositor.Plugins", "UnregisterPlugin", handle_unregister_plugin },
};

method_entry_t *get_plugins_method_entries() {
    return plugins_entries;
}

// Handlers
DBusHandlerResult handle_register_plugin(compositor_t *server, DBusMessage *msg) {
    DBusError error;
    dbus_error_init(&error);

    const char *plugin_name = NULL;
    const char *handler_type = NULL;
    dbus_int32_t pid_arg = 0;

    fde_log(FDE_INFO, "Called register plugin");

    if (!dbus_message_get_args(msg, &error,
                               DBUS_TYPE_STRING, &plugin_name,
                               DBUS_TYPE_STRING, &handler_type,
                               DBUS_TYPE_INT32, &pid_arg,
                               DBUS_TYPE_INVALID)) {
        fde_log(FDE_ERROR, "Invalid args in RegisterPlugin: %s", error.message);
        DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, error.message);
        dbus_connection_send(server->dbus_conn, reply, NULL);
        dbus_message_unref(reply);
        dbus_error_free(&error);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    plugin_instance_t *existing = plugin_list_find_by_name(server, plugin_name);
    if (existing) {
        // Обновляем существующий (временный) плагин
        free(existing->dbus_path);
        existing->dbus_path = malloc(64);
        if (existing->dbus_path) {
            snprintf(existing->dbus_path, 64, "/org/fde/plugin/%s", plugin_name);
        } else {
            fde_log(FDE_ERROR, "Failed to allocate dbus_path for plugin %s", plugin_name);
        }
        existing->pid = (pid_t)pid_arg;

        if (strcmp(handler_type, "input") == 0) existing->supports_input = true;
        else if (strcmp(handler_type, "rendering") == 0) existing->supports_rendering = true;
        else if (strcmp(handler_type, "protocols") == 0) existing->supports_protocols = true;
        else fde_log(FDE_INFO, "Unknown handler_type '%s' for %s", handler_type, plugin_name);

        fde_log(FDE_INFO, "Updated existing plugin %s (%s, PID %d)", plugin_name, handler_type, existing->pid);
    } else {
        // Создаём новый плагин, если не найден
        plugin_instance_t *new_plugin = calloc(1, sizeof(plugin_instance_t));
        if (!new_plugin) {
            DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_NO_MEMORY, "Out of memory");
            dbus_connection_send(server->dbus_conn, reply, NULL);
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_HANDLED;
        }
        new_plugin->pid = (pid_t)pid_arg;
        new_plugin->name = strdup(plugin_name);
        new_plugin->dbus_path = malloc(64);
        if (new_plugin->dbus_path) {
            snprintf(new_plugin->dbus_path, 64, "/org/fde/plugin/%s", plugin_name);
        } else {
            fde_log(FDE_ERROR, "Failed to allocate dbus_path for new plugin %s", plugin_name);
        }

        fde_log(FDE_INFO, "Allocated dbus_path: %s \nfor plugin: %s,", new_plugin->dbus_path, plugin_name);

        if (strcmp(handler_type, "input") == 0) new_plugin->supports_input = true;
        else if (strcmp(handler_type, "rendering") == 0) new_plugin->supports_rendering = true;
        else if (strcmp(handler_type, "protocols") == 0) new_plugin->supports_protocols = true;
        else fde_log(FDE_INFO, "Unknown handler_type '%s' for %s", handler_type, plugin_name);

        plugin_list_add(server, new_plugin);
        fde_log(FDE_INFO, "Registered new plugin %s (%s, PID %d)", plugin_name, handler_type, new_plugin->pid);
    }

    // Отправляем ответ с успехом
    DBusMessage *reply = dbus_message_new_method_return(msg);
    dbus_bool_t success = TRUE;
    dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &success, DBUS_TYPE_INVALID);
    dbus_connection_send(server->dbus_conn, reply, NULL);
    dbus_message_unref(reply);

    // Отправляем сигнал о регистрации плагина
    send_dbus_signal(
        server,
        "org.fde.Compositor.Core",
        "PluginRegistered",
        DBUS_TYPE_STRING, plugin_name,
        DBUS_TYPE_STRING, handler_type,
        DBUS_TYPE_INT32, pid_arg,
        DBUS_TYPE_INVALID
    );

    dbus_error_free(&error);
    return DBUS_HANDLER_RESULT_HANDLED;
}