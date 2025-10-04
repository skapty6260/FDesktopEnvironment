#include <fde/dbus.h>
#include <fde/plugin-system.h>
#include <fde/utils/log.h> 

#define CORE_INTERFACE "org.fde.Compositor.Core"

static method_entry_t core_entries[] = {
    { "org.fde.Compositor.Core", "GetProperty", handle_get_property },
    { "org.fde.Compositor.Core", "SetProperty", handle_set_property },
    { "org.fde.Compositor.Core", "Introspect", handle_introspect },
};

method_entry_t *get_core_method_entries() {
    return core_entries;
}

// Геттеры
static void get_num_plugins(compositor_t *s, void *val) { *(int *)val = wl_list_length(&s->plugins); }

// Таблица свойств (для Get/SetProperty; расширяйте: добавляйте для новых полей)
typedef struct {
    const char *name;            // e.g., "plugins.num"
    const char *type;            // "s" (string), "i" (int32), "b" (bool), "as" (array string)
    void (*getter)(compositor_t *server, void *value);  // Функция для получения значения
    bool (*setter)(compositor_t *server, void *value);  // Функция для установки (NULL если read-only)
} property_entry_t;

static property_entry_t property_entries[] = {
    { "plugins_num", "i", get_num_plugins, NULL },
    { NULL, NULL, NULL, NULL}
};

// Handlers
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

    // Поиск свойства в таблице
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

    // Получаем значение свойства
    // Для простоты здесь поддерживается только int32, bool и string
    DBusMessage *reply = dbus_message_new_method_return(msg);
    if (!reply) {
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    DBusMessageIter iter, variant_iter;
    dbus_message_iter_init_append(reply, &iter);

    if (strcmp(entry->type, "i") == 0) {
        int int_val = 0;
        entry->getter(server, &int_val);

        // Открываем контейнер variant с сигнатурой "i"
        if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "i", &variant_iter)) {
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }
        if (!dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_INT32, &int_val)) {
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }
        dbus_message_iter_close_container(&iter, &variant_iter);

    } else if (strcmp(entry->type, "b") == 0) {
        dbus_bool_t bool_val = FALSE;
        entry->getter(server, &bool_val);

        if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "b", &variant_iter)) {
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }
        if (!dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_BOOLEAN, &bool_val)) {
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }
        dbus_message_iter_close_container(&iter, &variant_iter);

    } else if (strcmp(entry->type, "s") == 0) {
        const char *str_val = NULL;
        entry->getter(server, (void *)&str_val);  // Предполагается, что геттер возвращает const char*

        if (!str_val) str_val = "";

        if (!dbus_message_iter_open_container(&iter, DBUS_TYPE_VARIANT, "s", &variant_iter)) {
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }
        if (!dbus_message_iter_append_basic(&variant_iter, DBUS_TYPE_STRING, &str_val)) {
            dbus_message_unref(reply);
            return DBUS_HANDLER_RESULT_NEED_MEMORY;
        }
        dbus_message_iter_close_container(&iter, &variant_iter);

    } else {
        // Не поддерживаемый тип
        dbus_message_unref(reply);
        DBusMessage *error_reply = dbus_message_new_error(msg, DBUS_ERROR_INVALID_ARGS, "Unsupported property type");
        dbus_connection_send(server->dbus_conn, error_reply, NULL);
        dbus_message_unref(error_reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }

    // Отправляем ответ
    dbus_connection_send(server->dbus_conn, reply, NULL);
    dbus_message_unref(reply);

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