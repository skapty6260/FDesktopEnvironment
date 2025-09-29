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

const method_entry_t **get_all_method_entries(void) {
    static const method_entry_t *entries[4] = { NULL };
    static bool initialized = false;
    if (!initialized) {
        entries[0] = get_core_method_entries();
        entries[1] = get_plugins_method_entries();
        entries[2] = get_config_method_entries();
        entries[3] = NULL;
        initialized = true;
    }
    return entries;
}
method_handler_t find_handler(const char *interface, const char *method) {
    const method_entry_t **all_method_entries = get_all_method_entries();
    for (int i = 0; all_method_entries[i] != NULL; i++) {
        const method_entry_t *entries = all_method_entries[i];
        for (int j = 0; entries[j].interface != NULL; j++) {
            if (strcmp(interface, entries[j].interface) == 0 &&
                strcmp(method, entries[j].method) == 0) {
                return entries[j].handler;
            }
        }
    }
    return NULL;
}

DBusHandlerResult dbus_message_filter(DBusConnection *conn, DBusMessage *msg, void *user_data) {
    compositor_t *server = (compositor_t *)user_data;
    if (!server || !server->dbus_conn) {
        return DBUS_HANDLER_RESULT_NEED_MEMORY;
    }

    // Проверяем, что это method_call
    if (dbus_message_get_type(msg) != DBUS_MESSAGE_TYPE_METHOD_CALL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;  // Игнорируем signals, replies, errors
    }

    const char *interface = dbus_message_get_interface(msg);
    const char *method = dbus_message_get_member(msg);
    const char *sender = dbus_message_get_sender(msg);

    if (!interface || !method) {
        fde_log(FDE_ERROR, "Invalid D-Bus message from %s: no interface/method", sender ?: "unknown");
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    // Безопасность: обрабатываем только свои интерфейсы
    if (strstr(interface, "org.fde.Compositor") == NULL) {
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    fde_log(FDE_DEBUG, "Received method call: %s.%s from %s", interface, method, sender ?: "unknown");

    // Ищем обработчик через find_handler
    method_handler_t handler = find_handler(interface, method);
    if (handler) {
        DBusHandlerResult result = handler(server, msg);
        dbus_connection_flush(conn);  // Отправляем ответ сразу
        return result;
    } else {
        // Метод не реализован
        DBusMessage *reply = dbus_message_new_error(msg, DBUS_ERROR_UNKNOWN_METHOD, "Unknown interface/method");
        dbus_connection_send(conn, reply, NULL);
        dbus_message_unref(reply);
        return DBUS_HANDLER_RESULT_HANDLED;
    }
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