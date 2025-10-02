#define _DEFAULT_SOURCE

#include <features.h>
#include <linux/limits.h>
#include <string.h>
#include <unistd.h>    // fork, execv, access
#include <sys/wait.h>  // waitpid (опционально)
#include <dirent.h>    // opendir, readdir
#include <sys/stat.h>  // stat
#include <limits.h>    // PATH_MAX
#include <sys/types.h> // Required for opendir and readdir
#include <stdlib.h>
#include <signal.h>

#include <fde/dbus.h>
#include <fde/utils/log.h>
#include <fde/config.h>
#include <fde/plugin-system.h>

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

bool load_plugins_from_dir(compositor_t *server, struct fde_config *config) {
    if (!server || !config || !config->plugins->dir) {
        fde_log(FDE_ERROR, "No config or plugins dir set");
        return false;
    }

    DIR *dir = opendir(config->plugins->dir);
    if (!dir) {
        fde_log(FDE_ERROR, "Cannot open plugins dir '%s': %s", config->plugins->dir, strerror(errno));
        return false;
    }

    fde_log(FDE_INFO, "Scanning plugins in '%s'", config->plugins->dir);
    struct dirent *entry;
    int launched_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' || strstr(entry->d_name, ".conf") || entry->d_type != DT_REG)
            continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", config->plugins->dir, entry->d_name);

        // Проверяем, что файл исполняемый
        if (access(path, X_OK) != 0) {
            fde_log(FDE_DEBUG, "Skipping non-executable: %s (%s)", entry->d_name, strerror(errno));
            continue;
        }

        pid_t pid = fork();
        if (pid < 0) {
            fde_log(FDE_ERROR, "Fork failed for %s: %s", entry->d_name, strerror(errno));
            continue;
        } else if (pid == 0) {
            // Дочерний процесс: exec плагин
            char *argv[] = {entry->d_name, NULL};
            execv(path, argv);
            fprintf(stderr, "Exec failed for %s: %s\n", path, strerror(errno));
            exit(1);
        }

        // Родительский процесс: добавляем временный плагин в список
        plugin_instance_t *temp_plugin = calloc(1, sizeof(plugin_instance_t));
        if (!temp_plugin) {
            fde_log(FDE_ERROR, "Cannot alloc temp plugin for %s", entry->d_name);
            kill(pid, SIGTERM);
            continue;
        }
        temp_plugin->pid = pid;
        temp_plugin->name = strdup(entry->d_name);
        temp_plugin->dbus_path = NULL;
        // Флаги по умолчанию: unknown
        plugin_list_add(server, temp_plugin);
        fde_log(FDE_INFO, "Launched plugin '%s' (PID %d) | (FOR DEBUG: Check for [src/dbus/dbus.c] Plugin register or update existing)", entry->d_name, pid);
        launched_count++;
    }

    closedir(dir);
    return launched_count > 0;
}

void plugin_instance_destroy(plugin_instance_t *plugin) {
    if (!plugin) return;

    fde_log(FDE_DEBUG, "Destroying plugin '%s' (PID %d)", plugin->name ? plugin->name : "unknown", plugin->pid);

    // Graceful shutdown: SIGTERM
    if (plugin->pid > 0) {
        kill(plugin->pid, SIGTERM);
        int status;
        // Ждём завершения (blocking; если нужно non-blocking — используйте waitpid с WNOHANG + loop)
        if (waitpid(plugin->pid, &status, 0) == -1) {
            fde_log(FDE_INFO, "waitpid failed for plugin PID %d: %s", plugin->pid, strerror(errno));
            // Force kill если не умер
            kill(plugin->pid, SIGKILL);
            waitpid(plugin->pid, &status, 0);  // Ещё раз wait
        } else {
            fde_log(FDE_DEBUG, "Plugin '%s' (PID %d) terminated with status %d", 
                    plugin->name ? plugin->name : "unknown", plugin->pid, WEXITSTATUS(status));
        }
    }

    // Free полей
    FREE_AND_NULL(plugin->name);
    FREE_AND_NULL(plugin->dbus_path);
    // Другие поля, если есть (e.g., free(plugin->some_data))

    free(plugin);
}
