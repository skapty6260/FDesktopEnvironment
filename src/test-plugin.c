// test-plugin.c: Тестовый плагин для FDE (регистрируется и получает num_plugins)
// Улучшенная версия: retry, long timeout, logging, interface="org.fde.Compositor.Input" (match XML)

#include <dbus/dbus.h>
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>  // getpid, sleep
#include <stdlib.h>  // exit

int main(int argc, char *argv[]) {
    fprintf(stderr, "Test plugin started (PID %d)\n", getpid());

    DBusError error;
    dbus_error_init(&error);

    // Подключение к session bus
    DBusConnection *conn = dbus_bus_get(DBUS_BUS_SESSION, &error);
    if (dbus_error_is_set(&error)) {
        fprintf(stderr, "Connection failed: %s\n", error.message);
        dbus_error_free(&error);
        return 1;
    }

    // Log env и unique name для debug
    fprintf(stderr, "Connected to session bus (unique: %s)\n", dbus_bus_get_unique_name(conn));
    fprintf(stderr, "DBUS_SESSION_BUS_ADDRESS: %s\n", getenv("DBUS_SESSION_BUS_ADDRESS") ?: "NULL");

    dbus_connection_flush(conn);

    const char *service = "org.fde.Compositor";
    const char *path = "/org/fde/Compositor";
    const char *interface = "org.fde.Compositor.Input";  // Match XML (Input interface)

    // Initial delay для fde dispatch/filter readiness
    fprintf(stderr, "Waiting 1s for compositor readiness...\n");
    sleep(1);

    // 1. Retry loop для RegisterPlugin (3 retries, 5s timeout each)
    bool registered = false;
    fprintf(stderr, "Attempting RegisterPlugin (3 retries, 5s timeout each)...\n");
    for (int retry = 0; retry < 3; retry++) {
        DBusMessage *msg = dbus_message_new_method_call(service, path, interface, "RegisterPlugin");
        if (!msg) {
            fprintf(stderr, "Cannot create RegisterPlugin message (retry %d/3)\n", retry + 1);
            sleep(1);
            continue;
        }

        const char *plugin_name = "test-plugin";
        const char *handler_type = "input";  // Match XML/Input
        dbus_int32_t pid = getpid();

        dbus_message_append_args(msg,
                                 DBUS_TYPE_STRING, &plugin_name,
                                 DBUS_TYPE_STRING, &handler_type,
                                 DBUS_TYPE_INT32, &pid,
                                 DBUS_TYPE_INVALID);

        DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, 5000, &error);  // 5s timeout
        dbus_message_unref(msg);

        if (dbus_error_is_set(&error)) {
            fprintf(stderr, "RegisterPlugin failed (retry %d/3): %s\n", retry + 1, error.message);
            dbus_error_free(&error);
            sleep(1);
            continue;
        }

        if (reply) {
            dbus_bool_t success;
            if (dbus_message_get_args(reply, NULL, DBUS_TYPE_BOOLEAN, &success, DBUS_TYPE_INVALID)) {
                printf("Plugin registered: %s\n", success ? "success" : "failed");
                if (success) {
                    registered = true;
                }
            }
            dbus_message_unref(reply);
        }

        if (registered) {
            fprintf(stderr, "RegisterPlugin success on retry %d/3\n", retry + 1);
            break;
        }
    }

    if (!registered) {
        fprintf(stderr, "All RegisterPlugin retries failed; exiting\n");
        goto cleanup;
    }

    // Flush и short delay перед GetProperty
    dbus_connection_flush(conn);
    sleep(1);

    // 2. Retry loop для GetProperty (3 retries, 5s timeout each)
    bool got_property = false;
    fprintf(stderr, "Attempting GetProperty (3 retries, 5s timeout each)...\n");
    for (int retry = 0; retry < 3; retry++) {
        DBusMessage *msg = dbus_message_new_method_call(service, path, "org.fde.Compositor.Core", "GetProperty");  // Core для properties
        if (!msg) {
            fprintf(stderr, "Cannot create GetProperty message (retry %d/3)\n", retry + 1);
            sleep(1);
            continue;
        }

        const char *prop_name = "plugins_num";
        dbus_message_append_args(msg, DBUS_TYPE_STRING, &prop_name, DBUS_TYPE_INVALID);

        DBusMessage *reply = dbus_connection_send_with_reply_and_block(conn, msg, 5000, &error);
        dbus_message_unref(msg);

        if (dbus_error_is_set(&error)) {
            fprintf(stderr, "GetProperty failed (retry %d/3): %s\n", retry + 1, error.message);
            dbus_error_free(&error);
            sleep(1);
            continue;
        }

        if (reply) {
            dbus_int32_t num;
            if (dbus_message_get_args(reply, NULL, DBUS_TYPE_INT32, &num, DBUS_TYPE_INVALID)) {
                printf("Number of plugins: %d\n", (int)num);
                got_property = true;
            }
            dbus_message_unref(reply);
        }

        if (got_property) {
            fprintf(stderr, "GetProperty success on retry %d/3\n", retry + 1);
            break;
        }
    }

    if (!got_property) {
        fprintf(stderr, "All GetProperty retries failed\n");
    }

    // Hold для logs/visibility
    sleep(2);
    printf("Test plugin exiting (registered OK)\n");

cleanup:
    if (conn) {
        dbus_connection_unref(conn);
    }
    dbus_error_free(&error);
    return 0;
}