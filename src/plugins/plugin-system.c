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

// TODO: Добавить поддержку ивентов в плагинах
// Отправлять dbus сигналы на все подключенные плагины при каких-либо ивентах 

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

static char *expand_tilde(const char *path) {
    if (!path || path[0] != '~') return strdup(path ? path : "");

    const char *home = getenv("HOME");
    if (!home) {
        home = "/home/user";  // fallback
    }
    size_t len = strlen(home) + strlen(path);
    char *full = malloc(len);
    if (!full) return NULL;
    snprintf(full, len, "%s%s", home, path + 1);
    return full;
}

bool load_plugins_from_dir(compositor_t *server, struct fde_config *config) {
    if (!server || !config || !config->plugins.dir) {
        fde_log(FDE_ERROR, "No config or plugins dir set");
        return false;
    }

    char *plugins_path = expand_tilde(config->plugins.dir);
    DIR *dir = opendir(plugins_path);
    if (!dir) {
        fde_log(FDE_ERROR, "Cannot open plugins dir '%s': %s", plugins_path, strerror(errno));
        return false;
    }

    fde_log(FDE_INFO, "Scanning plugins in '%s'", plugins_path);
    struct dirent *entry;
    int launched_count = 0;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' || strstr(entry->d_name, ".conf") || entry->d_type != DT_REG)
            continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", plugins_path, entry->d_name);

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
