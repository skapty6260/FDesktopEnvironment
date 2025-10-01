#pragma once

#include <stdbool.h>
#include <fde/comp/compositor.h>

typedef DBusHandlerResult (*method_handler_t)(compositor_t *server, DBusMessage *msg);

typedef struct {
    const char *interface;       // e.g., "org.fde.Compositor.Input"
    const char *method;          // e.g., "RegisterPlugin"
    method_handler_t handler;    // Функция-обработчик (callback, handler)
} method_entry_t;

bool init_dbus(compositor_t *server);
void cleanup_dbus(compositor_t *server);

DBusHandlerResult dbus_message_filter(DBusConnection *conn, DBusMessage *msg, void *user_data);

// Получение entries
method_handler_t find_handler(const char *interface, const char *method);

method_entry_t *get_config_method_entries(void);
method_entry_t *get_plugins_method_entries(void);
method_entry_t *get_core_method_entries(void);

// Handlers (примеры; добавляйте новые)
DBusHandlerResult handle_register_plugin(compositor_t *server, DBusMessage *msg);
DBusHandlerResult handle_get_property(compositor_t *server, DBusMessage *msg);
DBusHandlerResult handle_set_property(compositor_t *server, DBusMessage *msg);
DBusHandlerResult handle_inject_input(compositor_t *server, DBusMessage *msg);  // Пример для Input
DBusHandlerResult handle_introspect(compositor_t *server, DBusMessage *msg); // Introspection XML data

// Утилиты (для сигналов и т.д.)
void send_dbus_signal(compositor_t *server, const char *interface, const char *signal_name, ...);
// size_t get_num_plugins(compositor_t *server);  // Пример getter для свойств
int dbus_fd_handler(int fd, uint32_t mask, void *data);  // Исправленная сигнатура