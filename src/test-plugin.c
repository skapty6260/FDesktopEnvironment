#include <gio/gio.h>
#include <stdio.h>
#include <unistd.h> // getpid

#include "org.fde.Compositor.h"  // Сгенерированный заголовок из XML

int main(int argc, char *argv[]) {
    GError *error = NULL;

    // Подключаемся к сессионной шине
    GDBusConnection *connection = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if (!connection) {
        fprintf(stderr, "Failed to connect to session bus: %s\n", error->message);
        g_error_free(error);
        return 1;
    }

    // Создаём прокси для интерфейса org.fde.Compositor.Plugins
    OrgFdeCompositorOrgFdeCompositorPlugins *plugins_proxy =
        org_fde_compositor_org_fde_compositor_plugins_proxy_new_sync(
            connection,
            G_DBUS_PROXY_FLAGS_NONE,
            "org.fde.Compositor",
            "/org/fde/Compositor",
            NULL,
            &error);

    if (!plugins_proxy) {
        fprintf(stderr, "Failed to create plugins proxy: %s\n", error->message);
        g_error_free(error);
        g_object_unref(connection);
        return 1;
    }

    // Создаём прокси для интерфейса org.fde.Compositor.Core
    OrgFdeCompositorOrgFdeCompositorCore *core_proxy =
        org_fde_compositor_org_fde_compositor_core_proxy_new_sync(
            connection,
            G_DBUS_PROXY_FLAGS_NONE,
            "org.fde.Compositor",
            "/org/fde/Compositor",
            NULL,
            &error);

    if (!core_proxy) {
        fprintf(stderr, "Failed to create core proxy: %s\n", error->message);
        g_error_free(error);
        g_object_unref(plugins_proxy);
        g_object_unref(connection);
        return 1;
    }

    const char *plugin_name = "test-plugin";
    const char *handler_type = "input";
    gint pid = getpid();

    // Вызываем RegisterPlugin
    gboolean success = FALSE;
    if (!org_fde_compositor_org_fde_compositor_plugins_call_register_plugin_sync(
            plugins_proxy,
            plugin_name,
            handler_type,
            pid,
            &success,
            NULL,
            &error)) {
        fprintf(stderr, "RegisterPlugin call failed: %s\n", error->message);
        g_error_free(error);
        goto cleanup;
    }

    printf("RegisterPlugin success: %s\n", success ? "true" : "false");
    if (!success) {
        fprintf(stderr, "RegisterPlugin returned failure\n");
        goto cleanup;
    }

    // Вызываем GetProperty "plugins_num"
    GVariant *value = NULL;
    if (!org_fde_compositor_org_fde_compositor_core_call_get_property_sync(
            core_proxy,
            "plugins_num",
            &value,
            NULL,
            &error)) {
        fprintf(stderr, "GetProperty call failed: %s\n", error->message);
        g_error_free(error);
        goto cleanup;
    }

    if (value) {
        GVariant *inner = NULL;
        g_variant_get(value, "v", &inner);  // <-- исправлено с "(v)" на "v"
        gint32 num_plugins = 0;
        g_variant_get(inner, "i", &num_plugins);
        printf("Number of plugins: %d\n", num_plugins);
        g_variant_unref(inner);
        g_variant_unref(value);
    }

cleanup:
    g_object_unref(core_proxy);
    g_object_unref(plugins_proxy);
    g_object_unref(connection);
    return 0;
}