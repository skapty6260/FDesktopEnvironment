#pragma once

#include <dbus/dbus.h>
#include <stdbool.h>
#include <fde/compositor.h>

bool init_dbus(compositor_t *server);
void cleanup_dbus(compositor_t *server);

DBusHandlerResult dbus_message_filter(DBusConnection *conn, DBusMessage *msg, void *user_data);

// Handlers (примеры; добавляйте новые)
DBusHandlerResult handle_register_plugin(compositor_t *server, DBusMessage *msg);
DBusHandlerResult handle_get_property(compositor_t *server, DBusMessage *msg);
DBusHandlerResult handle_set_property(compositor_t *server, DBusMessage *msg);
DBusHandlerResult handle_inject_input(compositor_t *server, DBusMessage *msg);  // Пример для Input

// Утилиты (для сигналов и т.д.)
void send_dbus_signal(compositor_t *server, const char *interface, const char *signal_name, ...);
// size_t get_num_plugins(compositor_t *server);  // Пример getter для свойств
int dbus_fd_handler(int fd, uint32_t mask, void *data);  // Исправленная сигнатура