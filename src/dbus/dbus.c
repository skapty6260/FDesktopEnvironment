#include <fde/dbus.h>
#include <fde/compositor.h>
#include <fde/plugin-system.h>
#include <fde/utils/log.h> 
#include <dbus/dbus.h>
#include <wayland-util.h>
#include <signal.h> 
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>

// Таблица методов (расширяйте здесь: добавляйте entries для новых интерфейсов/методов)
typedef DBusHandlerResult (*method_handler_t)(compositor_t *server, DBusMessage *msg);

typedef struct {
    const char *interface;       // e.g., "org.fde.Compositor.Input"
    const char *method;          // e.g., "RegisterPlugin"
    method_handler_t handler;    // Функция-обработчик e.g., callback
} method_entry_t;

// TODO: refactor methods and handlers to different files for each interface (plugins, core, config)
static method_entry_t method_entries[] = {
    { "org.fde.Compositor.Plugins", "RegisterPlugin", handle_register_plugin },
    // { "org.fde.Compositor.Plugins", "UnregisterPlugin", handle_unregister_plugin },
    { "org.fde.Compositor.Core", "GetProperty", handle_get_property },
    { "org.fde.Compositor.Core", "SetProperty", handle_set_property },
    { "org.fde.Compositor.Core", "Introspect", handle_introspect },
    // Конец таблицы (sentinel)
    { NULL, NULL, NULL }
};

// Таблица свойств (для Get/SetProperty; расширяйте: добавляйте для новых полей)
typedef struct {
    const char *name;            // e.g., "plugins.num"
    const char *type;            // "s" (string), "i" (int32), "b" (bool), "as" (array string)
    void (*getter)(compositor_t *server, void *value);  // Функция для получения значения
    bool (*setter)(compositor_t *server, void *value);  // Функция для установки (NULL если read-only)
} property_entry_t;

// Геттеры композитора
static void get_num_plugins(compositor_t *s, void *val) { *(int *)val = wl_list_length(&s->plugins); }

static property_entry_t property_entries[] = {
    { "plugins_num", "i", get_num_plugins, NULL },
    { NULL, NULL, NULL, NULL}
};

// Фильтр сообщений D-Bus (главный диспетчер)
DBusHandlerResult dbus_message_filter(DBusConnection *conn, DBusMessage *msg, void *user_data) {
    fde_log(FDE_INFO, "Called DBus filter.");

    compositor_t *server = (compositor_t *)user_data;
    if (!server || !server->dbus_conn) {
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    // Проверяем, является ли сообщение method_call (любым)
    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;  // Игнорируем signals, replies, errors
    }

    fde_log(FDE_INFO, "D-Bus filter called with method call: %d", dbus_message_get_type(msg));

    const char *type_str = "unknown";
         int type = dbus_message_get_type(msg);
         if (type == DBUS_MESSAGE_TYPE_METHOD_CALL) type_str = "method_call";
         else if (type == DBUS_MESSAGE_TYPE_SIGNAL) type_str = "signal";
         else if (type == DBUS_MESSAGE_TYPE_METHOD_RETURN) type_str = "method_return";
         else if (type == DBUS_MESSAGE_TYPE_ERROR) type_str = "error";

    const char *interface = dbus_message_get_interface(msg);
    const char *method = dbus_message_get_member(msg);
    const char *sender = dbus_message_get_sender(msg);  // Для безопасности/логов
    if (!interface || !method) {
        fde_log(FDE_ERROR, "Invalid D-Bus message from %s: no interface/method", sender ?: "unknown");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // Только для вашего сервиса (безопасность: игнорируем чужие интерфейсы)
    if (strstr(interface, "org.fde.Compositor") == NULL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    fde_log(FDE_INFO, "Received method call: %s.%s from %s", interface, method, sender ?: "unknown");

    // Диспетчеризация по таблице (расширяемо)
    for (int i = 0; method_entries[i].interface != NULL; ++i) {
        if (strcmp(interface, method_entries[i].interface) == 0 &&
            strcmp(method, method_entries[i].method) == 0) {
            if (method_entries[i].handler) {
                DBusHandlerResult result = method_entries[i].handler(server, msg);
                dbus_connection_flush(conn);  // Отправляем ответ сразу
                return result;
            } else {
                // Метод не реализован
                DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_METHOD, "Method not implemented");
                dbus_connection_send(conn, reply, NULL);
                dbus_message_unref(reply);
                return DBUS_HANDLER_RESULT_HANDLED;
            }
        }
    }

    // Неизвестный метод
    DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_METHOD, "Unknown interface/method");
    dbus_connection_send(conn, reply, NULL);
    dbus_message_unref(reply);
    return DBUS_HANDLER_RESULT_HANDLED;
}

static void dbus_free_server_data(void *data) {
    (void)data;  // No-op
    fde_log(FDE_DEBUG, "Free server dbus data (no-op)");
}

bool init_dbus(compositor_t *server) {
    if (!server) return false;

    wl_list_init(&server->plugins);

    DBusError error;
    dbus_error_init(&error);

    // Подключение к сессионному bus (Wayland)
    server->dbus_conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set(&error)) {
        fde_log(FDE_ERROR, "D-Bus connection failed: %s", error.message);
        dbus_error_free(&error);
        return false;
    }
    if (server->dbus_conn == NULL) {
        fde_log(FDE_ERROR, "D-Bus connection is NULL");
        return false;
    }

    const char *match_rule = "type='method_call',interface='org.fde.Compositor.Core'";
    dbus_bus_add_match(server->dbus_conn, match_rule, &error);
    dbus_connection_flush(server->dbus_conn);
    if (dbus_error_is_set(&error)) {
        fde_log(FDE_ERROR, "Failed to add match rule: %s", error.message);
        dbus_error_free(&error);
        return false;
    }

    // Запрос уникального имени сервиса
    server->dbus_service_name = strdup("org.fde.Compositor");
    int request_result = dbus_bus_request_name(
        server->dbus_conn, server->dbus_service_name,
        DBUS_NAME_FLAG_DO_NOT_QUEUE | DBUS_NAME_FLAG_REPLACE_EXISTING,
        &error
    );
    if (request_result != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        fde_log(FDE_ERROR, "Failed to acquire D-Bus name '%s': %s (code %d)", server->dbus_service_name, error.message, request_result);
        dbus_error_free(&error);
        return false;
    }

    // Добавление фильтра для входящих сообщений
    dbus_connection_add_filter(server->dbus_conn, dbus_message_filter, server, dbus_free_server_data);
    dbus_connection_flush(server->dbus_conn);  // Flush: Активируем filter

    // Initial dispatch: Process any queued (helps if early messages)
    dbus_connection_read_write(server->dbus_conn, 0);  // Non-blocking read/write
    while (dbus_connection_dispatch(server->dbus_conn) == DBUS_DISPATCH_DATA_REMAINS) {
        // Process all pending
    }
    
    fde_log(FDE_INFO, "D-Bus initialized: service '%s' on session bus", server->dbus_service_name);
    dbus_error_free(&error);
    return true;
}
void cleanup_dbus(compositor_t *server) {
    if (!server) return;
    // Удаление фильтра
    dbus_connection_remove_filter(server->dbus_conn, dbus_message_filter, server);
    // Убить плагины (безопасная итерация wl_list)
    plugin_instance_t *p, *tmp;
    wl_list_for_each_safe(p, tmp, &server->plugins, link) {
        if (p->pid > 0) {
            kill(p->pid, SIGTERM);
            fde_log(FDE_INFO, "Terminated plugin %s (PID %d)", p->name ?: "unknown", p->pid);
        }
        wl_list_remove(&p->link);
        free(p->name);
        free(p->dbus_path);
        free(p);
    }
    // Закрытие соединения
    if (server->dbus_conn) {
        dbus_connection_flush(server->dbus_conn);
        dbus_connection_unref(server->dbus_conn);
        server->dbus_conn = NULL;
    }
    free(server->dbus_service_name);
    server->dbus_service_name = NULL;
    fde_log(FDE_INFO, "D-Bus cleaned up");
}

// TODO: refactor handlers to different files
// Register plugin handler
DBusHandlerResult handle_register_plugin(compositor_t *server, DBusMessage *msg) {
    DBusError error;
    dbus_error_init(&error);

    const char *plugin_name = NULL;
    const char *handler_type = NULL;
    dbus_int32_t pid_arg = 0;

    fde_log(FDE_INFO, "Called register plugin");

    if (!dbus_message_get_args(msg, &error, DBUS_TYPE_STRING, &plugin_name, DBUS_TYPE_STRING, &handler_type, DBUS_TYPE_INT32, &pid_arg, DBUS_TYPE_INVALID)) {
        fde_log(FDE_ERROR, "Invalid args in RegisterPlugin: %s", error.message);
        DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, error.message);
        dbus_connection_send(server->dbus_conn, reply, NULL);
        dbus_message_unref(reply);
        dbus_error_free(&error);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    plugin_instance_t *existing = plugin_list_find_by_name(server, plugin_name);
    if (existing) {
        // Обновляем существующий плагин
        free(existing->dbus_path);
        existing->dbus_path = malloc(64);
        snprintf(existing->dbus_path, 64, "/org/fde/plugin/%s", plugin_name);
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
        snprintf(new_plugin->dbus_path, 64, "/org/fde/plugin/%s", plugin_name);

        if (strcmp(handler_type, "input") == 0) new_plugin->supports_input = true;
        else if (strcmp(handler_type, "rendering") == 0) new_plugin->supports_rendering = true;
        else if (strcmp(handler_type, "protocols") == 0) new_plugin->supports_protocols = true;
        else fde_log(FDE_INFO, "Unknown handler_type '%s' for %s", handler_type, plugin_name);

        plugin_list_add(server, new_plugin);
        fde_log(FDE_INFO, "Registered new plugin %s (%s, PID %d)", plugin_name, handler_type, new_plugin->pid);
    }

    DBusMessage *reply = dbus_message_new_method_return(msg);
    dbus_bool_t success = TRUE;

    dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &success, DBUS_TYPE_INVALID);
    dbus_connection_send(server->dbus_conn, reply, NULL);
    dbus_message_unref(reply);

    send_dbus_signal(server, "org.fde.Compositor.Core", "PluginRegistered", DBUS_TYPE_STRING, &plugin_name, DBUS_TYPE_INVALID);

    dbus_error_free(&error);
    return DBUS_HANDLER_RESULT_HANDLED;
}

// Реализация handlers (примеры; расширяйте)
DBusHandlerResult handle_introspect(compositor_t *server, DBusMessage *msg) {
    if (!server || !server->dbus_conn) {
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    // Проверяем, что это Introspect (no args)
    DBusError error;
    dbus_error_init(&error);
    if (dbus_message_get_args(msg, &error, DBUS_TYPE_INVALID)) {  // No args expected
        // OK
    } else {
        fde_log(FDE_ERROR, "Invalid args in Introspect: %s", error.message);
        DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, error.message);
        dbus_connection_send(server->dbus_conn, reply, NULL);
        dbus_message_unref(reply);
        dbus_error_free(&error);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    dbus_error_free(&error);

    // XML introspection data (embedded из вашего файла; escape'нут для C)
    const char *xml_introspect = 
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE node PUBLIC \"-//freedesktop//DTD D-BUS Object Introspection 1.0//EN\"\n"
        "\"http://www.freedesktop.org/standards/dbus/1.0/introspect.dtd\">\n"
        "<node name=\"/org/fde/Compositor\">\n"
        "  <!-- Core interface: Basic methods for plugin registration and properties -->\n"
        "  <interface name=\"org.fde.Compositor.Core\">\n"
        "    <method name=\"RegisterPlugin\">\n"
        "      <arg type=\"s\" name=\"plugin_name\" direction=\"in\"/>\n"
        "      <arg type=\"s\" name=\"handler_type\" direction=\"in\"/>\n"
        "      <arg type=\"i\" name=\"pid\" direction=\"in\"/>\n"
        "      <arg type=\"b\" name=\"success\" direction=\"out\"/>\n"
        "    </method>\n"
        "    <method name=\"GetProperty\">\n"
        "      <arg type=\"s\" name=\"property_name\" direction=\"in\"/>\n"
        "      <arg type=\"v\" name=\"value\" direction=\"out\"/>\n"
        "    </method>\n"
        "    <method name=\"SetProperty\">\n"
        "      <arg type=\"s\" name=\"property_name\" direction=\"in\"/>\n"
        "      <arg type=\"v\" name=\"value\" direction=\"in\"/>\n"
        "      <arg type=\"b\" name=\"success\" direction=\"out\"/>\n"
        "    </method>\n"
        "    <signal name=\"PluginRegistered\">\n"
        "      <arg type=\"s\" name=\"plugin_name\"/>\n"
        "      <arg type=\"s\" name=\"handler_type\"/>\n"
        "      <arg type=\"i\" name=\"pid\"/>\n"
        "    </signal>\n"
        "  </interface>\n"
        "\n"
        "  <interface name=\"org.fde.Compositor.Input\">\n"
        "    <method name=\"InjectInputEvent\">\n"
        "      <arg type=\"i\" name=\"event_type\" direction=\"in\"/>\n"
        "      <arg type=\"s\" name=\"data\" direction=\"in\"/>\n"
        "      <arg type=\"b\" name=\"handled\" direction=\"out\"/>\n"
        "    </method>\n"
        "    <signal name=\"InputEventReceived\">\n"
        "      <arg type=\"i\" name=\"event_type\"/>\n"
        "      <arg type=\"s\" name=\"data\"/>\n"
        "    </signal>\n"
        "  </interface>\n"
        "\n"
        "  <interface name=\"org.fde.Compositor.Rendering\">\n"
        "    <method name=\"RegisterRenderer\">\n"
        "      <arg type=\"s\" name=\"plugin_name\" direction=\"in\"/>\n"
        "      <arg type=\"s\" name=\"render_mode\" direction=\"in\"/>\n"
        "      <arg type=\"b\" name=\"success\" direction=\"out\"/>\n"
        "    </method>\n"
        "    <method name=\"UpdateScene\">\n"
        "      <arg type=\"s\" name=\"scene_delta\" direction=\"in\"/>\n"
        "      <arg type=\"b\" name=\"applied\" direction=\"out\"/>\n"
        "    </method>\n"
        "    <signal name=\"FrameReady\">\n"
        "      <arg type=\"i\" name=\"timestamp\"/>\n"
        "    </signal>\n"
        "  </interface>\n"
        "\n"
        "  <interface name=\"org.fde.Compositor.Protocols\">\n"
        "    <method name=\"AddProtocol\">\n"
        "      <arg type=\"s\" name=\"protocol_name\" direction=\"in\"/>\n"
        "      <arg type=\"s\" name=\"impl_path\" direction=\"in\"/>\n"
        "      <arg type=\"b\" name=\"added\" direction=\"out\"/>\n"
        "    </method>\n"
        "  </interface>\n"
        "\n"
        "  <!-- Добавьте <interface> для Clients, Scene и т.д. при расширении -->\n"
        "</node>\n";

    // Создаём reply с XML
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) {
        fde_log(FDE_ERROR, "Failed to create Introspect reply (no memory)");
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    dbus_bool_t success = dbus_message_append_args(reply, DBUS_TYPE_STRING, &xml_introspect, DBUS_TYPE_INVALID);
    if (!success) {
        fde_log(FDE_ERROR, "Failed to append XML to Introspect reply");
        DBusMessage *error_reply = dbus_message_new_error(msg, DBUS_ERROR_NO_MEMORY, "Failed to append XML");
        dbus_connection_send(server->dbus_conn, error_reply, NULL);
        dbus_message_unref(error_reply);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // Send + flush (ensure immediate delivery)
    dbus_connection_send(server->dbus_conn, reply, NULL);
    dbus_connection_flush(server->dbus_conn);  // Critical: Flush для timely reply
    dbus_message_unref(reply);

    fde_log(FDE_DEBUG, "Introspect reply sent (XML length: %zu bytes)", strlen(xml_introspect));
    return DBUS_HANDLER_RESULT_HANDLED;
}
DBusHandlerResult handle_get_property(compositor_t *server, DBusMessage *msg) {
    DBusError error;
    dbus_error_init(&error);

    const char *prop_name = NULL;
    if (!dbus_message_get_args(msg, &error, DBUS_TYPE_STRING, &prop_name, DBUS_TYPE_INVALID)) {
        DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, error.message);
        dbus_connection_send(server->dbus_conn, reply, NULL);
        dbus_message_unref(reply);
        dbus_error_free(&error);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // Whitelist: Только известные свойства (безопасность)
    property_entry_t *entry = NULL;
    for (int i = 0; property_entries[i].name != NULL; ++i) {
        if (strcmp(prop_name, property_entries[i].name) == 0) {
            entry = &property_entries[i];
            break;
        }
    }
    if (!entry || !entry->getter) {
        DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, "Unknown or read-only property");
        dbus_connection_send(server->dbus_conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // Вызов getter и подготовка значения
    void *value = dbus_malloc0(1024);  // Буфер для значения (адаптируйте размер)
    if (!value) {
        DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_NO_MEMORY, "Out of memory");
        dbus_connection_send(server->dbus_conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    entry->getter(server, value);

    // Ответ с значением (упрощённо для базовых типов; для array используйте iter)
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (strcmp(entry->type, "i") == 0) {
        dbus_int32_t *int_val = (dbus_int32_t *)value;
        dbus_message_append_args(reply, DBUS_TYPE_INT32, int_val, DBUS_TYPE_INVALID);
    } else if (strcmp(entry->type, "b") == 0) {
        dbus_bool_t *bool_val = (dbus_bool_t *)value;
        dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, bool_val, DBUS_TYPE_INVALID);
    } else if (strcmp(entry->type, "s") == 0) {
        char **str_val = (char **)value;
        dbus_message_append_args(reply, DBUS_TYPE_STRING, str_val, DBUS_TYPE_INVALID);
    } else if (strcmp(entry->type, "as") == 0) {
        // Для array: используйте dbus_message_iter_append_fixed_array (упрощённо)
        char ***array_val = (char ***)value;
        DBusMessageIter iter;
        dbus_message_iter_init_append(reply, &iter);
        dbus_message_iter_append_fixed_array(&iter, DBUS_TYPE_STRING, array_val, -1);  // -1 для null-terminated
    }
    dbus_connection_send(server->dbus_conn, reply, NULL);
    dbus_message_unref(reply);
    dbus_free(value);  // Освобождаем буфер

    fde_log(FDE_DEBUG, "Returned property '%s'", prop_name);
    dbus_error_free(&error);
    return DBUS_HANDLER_RESULT_HANDLED;
}
DBusHandlerResult handle_set_property(compositor_t *server, DBusMessage *msg) {
    DBusError error;
    dbus_error_init(&error);

    const char *prop_name = NULL;
    if (!dbus_message_get_args(msg, &error, DBUS_TYPE_STRING, &prop_name, DBUS_TYPE_INVALID)) {
        DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, error.message);
        dbus_connection_send(server->dbus_conn, reply, NULL);
        dbus_message_unref(reply);
        dbus_error_free(&error);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // Whitelist: Только известные свойства
    property_entry_t *entry = NULL;
    for (int i = 0; property_entries[i].name != NULL; ++i) {
        if (strcmp(prop_name, property_entries[i].name) == 0) {
            entry = &property_entries[i];
            break;
        }
    }
    if (!entry || !entry->setter) {
        DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, "Unknown or read-only property");
        dbus_connection_send(server->dbus_conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // Получение значения из сообщения (упрощённо; для complex используйте iter)
    void *value = dbus_malloc0(1024);
    if (!value) {
        DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_NO_MEMORY, "Out of memory");
        dbus_connection_send(server->dbus_conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
    DBusMessageIter iter;
    dbus_message_iter_init(msg, &iter);
    dbus_message_iter_next(&iter);  // Пропустить prop_name (первый arg)
    int type = dbus_message_iter_get_arg_type(&iter);
    if (type == DBUS_TYPE_INT32) {
        dbus_int32_t int_val;
        dbus_message_iter_get_basic(&iter, &int_val);
        *(int *)value = int_val;
    } else if (type == DBUS_TYPE_BOOLEAN) {
        dbus_bool_t bool_val;
        dbus_message_iter_get_basic(&iter, &bool_val);
        *(bool *)value = bool_val;
    } else if (type == DBUS_TYPE_STRING) {
        const char *str_val;
        dbus_message_iter_get_basic(&iter, &str_val);
        strcpy((char *)value, str_val);
    } else {
        dbus_free(value);
        DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, "Unsupported value type");
        dbus_connection_send(server->dbus_conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // Вызов setter
    bool success = entry->setter(server, value);
    dbus_free(value);

    // Ответ
    DBusMessage *reply = dbus_message_new_method_return(msg);
    dbus_message_append_args(reply, DBUS_TYPE_BOOLEAN, &success, DBUS_TYPE_INVALID);
    dbus_connection_send(server->dbus_conn, reply, NULL);
    dbus_message_unref(reply);

    fde_log(FDE_DEBUG, "Set property '%s' to %s", prop_name, success ? "success" : "failed");
    dbus_error_free(&error);
    return DBUS_HANDLER_RESULT_HANDLED;
}
// DBusHandlerResult handle_inject_input(compositor_t *server, DBusMessage *msg) {
//     DBusError error;
//     dbus_error_init(&error);

//     // Пример args: string "keyboard", int32 keycode, bool pressed
//     const char *input_type = NULL;
//     dbus_int32_t keycode = 0;
//     dbus_bool_t pressed = FALSE;

//     if (!dbus_message_get_args(msg, &error,
//                                DBUS_TYPE_STRING, &input_type,
//                                DBUS_TYPE_INT32, &keycode,
//                                DBUS_TYPE_BOOLEAN, &pressed,
//                                DBUS_TYPE_INVALID)) {
//         fde_log(FDE_ERROR, "Invalid args in InjectInputEvent: %s", error.message);
//         DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, error.message);
//         dbus_connection_send(server->dbus_conn, reply, NULL);
//         dbus_message_unref(reply);
//         dbus_error_free(&error);
//         return DBUS_HANDLER_RESULT_HANDLED;
//     }

//     // TODO: Интегрируйте с wlroots input (e.g., wlr_keyboard_notify_key)
//     // Пример: if (strcmp(input_type, "keyboard") == 0) { wlr_keyboard_notify_key(server->keyboard, pressed ? WL_KEYBOARD_KEY_STATE_PRESSED : WL_KEYBOARD_KEY_STATE_RELEASED, keycode, time); }
//     fde_log(FDE_INFO, "Injected input: %s key %d %s", input_type, keycode, pressed ? "pressed" : "released");

//     // Успешный ответ
//     DBusMessage *reply = dbus_message_new_method_return(msg);
//     dbus_connection_send(server->dbus_conn, reply, NULL);
//     dbus_message_unref(reply);

//     dbus_error_free(&error);
//     return DBUS_HANDLER_RESULT_HANDLED;
// }
// DBusHandlerResult handle_update_scene(compositor_t *server, DBusMessage *msg) {
//     DBusError error;
//     dbus_error_init(&error);

//     // Пример args: string "plugin_name", double x, double y (позиция surface)
//     const char *plugin_name = NULL;
//     double x = 0.0, y = 0.0;

//     if (!dbus_message_get_args(msg, &error,
//                                DBUS_TYPE_STRING, &plugin_name,
//                                DBUS_TYPE_DOUBLE, &x,
//                                DBUS_TYPE_DOUBLE, &y,
//                                DBUS_TYPE_INVALID)) {
//         fde_log(FDE_ERROR, "Invalid args in UpdateScene: %s", error.message);
//         DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, error.message);
//         dbus_connection_send(server->dbus_conn, reply, NULL);
//         dbus_message_unref(reply);
//         dbus_error_free(&error);
//         return DBUS_HANDLER_RESULT_HANDLED;
//     }

//     // TODO: Интегрируйте с wlroots rendering (e.g., wlr_scene_set_position для plugin surface)
//     fde_log(FDE_INFO, "Updated scene for plugin '%s' to position (%.2f, %.2f)", plugin_name, x, y);

//     // Успешный ответ
//     DBusMessage *reply = dbus_message_new_method_return(msg);
//     dbus_connection_send(server->dbus_conn, reply, NULL);
//     dbus_message_unref(reply);

//     dbus_error_free(&error);
//     return DBUS_HANDLER_RESULT_HANDLED;
// }
// DBusHandlerResult handle_add_protocol(compositor_t *server, DBusMessage *msg) {
//     DBusError error;
//     dbus_error_init(&error);

//     // Пример args: string "xdg-shell-v1"
//     const char *protocol_name = NULL;

//     if (!dbus_message_get_args(msg, &error,
//                                DBUS_TYPE_STRING, &protocol_name,
//                                DBUS_TYPE_INVALID)) {
//         fde_log(FDE_ERROR, "Invalid args in AddProtocol: %s", error.message);
//         DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, error.message);
//         dbus_connection_send(server->dbus_conn, reply, NULL);
//         dbus_message_unref(reply);
//         dbus_error_free(&error);
//         return DBUS_HANDLER_RESULT_HANDLED;
//     }

//     // TODO: Интегрируйте с wayland-protocols (e.g., bind protocol handler)
//     fde_log(FDE_INFO, "Added protocol '%s'", protocol_name);

//     // Успешный ответ
//     DBusMessage *reply = dbus_message_new_method_return(msg);
//     dbus_connection_send(server->dbus_conn, reply, NULL);
//     dbus_message_unref(reply);

//     dbus_error_free(&error);
//     return DBUS_HANDLER_RESULT_HANDLED;
// }

// Утилита для отправки сигналов (broadcast; расширяема с va_list)
void send_dbus_signal(compositor_t *server, const char *interface, const char *signal_name, ...) {
    if (!server || !server->dbus_conn) return;
    DBusMessage *signal_msg = dbus_message_new_signal("/org/fde/Compositor", interface, signal_name);
    if (!signal_msg) return;
    va_list args;
    va_start(args, signal_name);
    DBusMessageIter iter;
    dbus_message_iter_init_append(signal_msg, &iter);
    int arg_type;
    while ((arg_type = va_arg(args, int)) != DBUS_TYPE_INVALID) {
        switch (arg_type) {
            case DBUS_TYPE_STRING: {
                const char *str = va_arg(args, const char *);
                if (str) dbus_message_iter_append_basic(&iter, DBUS_TYPE_STRING, &str);
                break;
            }
            case DBUS_TYPE_INT32: {
                dbus_int32_t val = va_arg(args, dbus_int32_t);
                dbus_message_iter_append_basic(&iter, DBUS_TYPE_INT32, &val);
                break;
            }
            case DBUS_TYPE_BOOLEAN: {
                dbus_bool_t val = va_arg(args, dbus_bool_t);
                dbus_message_iter_append_basic(&iter, DBUS_TYPE_BOOLEAN, &val);
                break;
            }
            default:
                fde_log(FDE_INFO, "Unsupported arg type %d", arg_type);
                break;
        }
    }
    va_end(args);
    dbus_connection_send(server->dbus_conn, signal_msg, NULL);
    dbus_connection_flush(server->dbus_conn);
    dbus_message_unref(signal_msg);
    fde_log(FDE_DEBUG, "Sent signal %s.%s", interface, signal_name);
}

int dbus_fd_handler(int fd, uint32_t mask, void *data) {
    compositor_t *server = (compositor_t *)data;
    if (!server || !server->dbus_conn) {
        fde_log(FDE_DEBUG, "D-Bus handler: invalid server or conn (fd=%d)", fd);
        return 0;  // Игнорируем, если нет соединения
    }

    dbus_connection_read_write(server->dbus_conn, 0);
    while (dbus_connection_get_dispatch_status(server->dbus_conn) == DBUS_DISPATCH_DATA_REMAINS) {
        dbus_connection_dispatch(server->dbus_conn);
    }

    // WL_EVENT_READABLE: Входящие данные (читаем сообщения)
    if (mask & WL_EVENT_READABLE) {
        dbus_connection_read_write(server->dbus_conn, 0);  // 0 = non-blocking (timeout 0 ms)
        // CRITICAL: Dispatch после read (process messages, call filter)
        DBusDispatchStatus status = dbus_connection_dispatch(server->dbus_conn);
        if (status == DBUS_DISPATCH_COMPLETE) {
            fde_log(FDE_DEBUG, "D-Bus dispatch: complete (messages processed)");
        } else if (status == DBUS_DISPATCH_DATA_REMAINS) {
            fde_log(FDE_DEBUG, "D-Bus dispatch: data remains (more to process next poll)");
        } else if (status == DBUS_DISPATCH_NEED_MEMORY) {
            fde_log(FDE_DEBUG, "D-Bus dispatch: need memory (retry later)");
        }
    }
   
    // WRITABLE: Исходящие (replies/signals) — flush queue
    if (mask & WL_EVENT_WRITABLE) {
        dbus_connection_read_write(server->dbus_conn, 0);
        // fde_log(FDE_DEBUG, "D-Bus writable: flushed outgoing");
    }

    // HANGUP/ERROR: Disconnect (bus down или pipe error)
    if (mask & (WL_EVENT_HANGUP | WL_EVENT_ERROR)) {
        fde_log(FDE_ERROR, "D-Bus fd error/hangup (fd=%d, mask=0x%x)", fd, mask);
        if (server->dbus_conn) {
            dbus_connection_unref(server->dbus_conn);
            server->dbus_conn = NULL;
        }

        // Опционально: Reconnect logic (но для простоты — disable)
        return 0;  // Loop удалит source
    }

    return 0;
}