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
    int timeout_sec = 10;  // Timeout для регистрации (простой; улучшите на timer)

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.' || strstr(entry->d_name, ".conf") || entry->d_type != DT_REG)
            continue;

        char path[PATH_MAX];
        snprintf(path, sizeof(path), "%s/%s", config->plugins->dir, entry->d_name);

        // Check executable
        if (access(path, X_OK) != 0) {
            fde_log(FDE_DEBUG, "Skipping non-executable: %s (%s)", entry->d_name, strerror(errno));
            continue;
        }

        // Fork и exec плагина
        pid_t pid = fork();
        if (pid < 0) {
            fde_log(FDE_ERROR, "Fork failed for %s: %s", entry->d_name, strerror(errno));
            continue;
        } else if (pid == 0) {
            // Child: Exec плагин (с правильным argv: non-null array)
            char *argv[] = {entry->d_name, NULL};  // argv[0] = имя программы (стандарт)
            execv(path, argv);
            // Если execv вернётся (ошибка), логируем и exit
            fprintf(stderr, "Exec failed for %s: %s\n", path, strerror(errno));
            exit(1);
        }

        // Parent: Временная запись в список (ожидаем регистрации)
        plugin_instance_t *temp_plugin = calloc(1, sizeof(plugin_instance_t));
        if (!temp_plugin) {
            fde_log(FDE_ERROR, "Cannot alloc temp plugin for %s", entry->d_name);
            kill(pid, SIGTERM);
            continue;
        }
        temp_plugin->pid = pid;
        temp_plugin->name = strdup(entry->d_name);  // Имя = basename
        temp_plugin->dbus_path = NULL;  // Заполнится при регистрации
        // Флаги по умолчанию: unknown
        plugin_list_add(server, temp_plugin);
        fde_log(FDE_INFO, "Launched plugin '%s' (PID %d); waiting for D-Bus registration...", entry->d_name, pid);
        launched_count++;

        // Простой timeout: Ждём регистрации (проверяем, обновилось ли имя/флаги)
        bool registered = false;
        for (int t = 0; t < timeout_sec; t++) {
            sleep(1);
            plugin_instance_t *found = plugin_list_find_by_name(server, entry->d_name);
            fde_log(FDE_DEBUG, "Check %d: found=%p, dbus_path=%s, supports=%d", t, found, found ? found->dbus_path : "NULL", 
                 found ? (found->supports_input + found->supports_rendering + found->supports_protocols) : 0);
            if (found && found->dbus_path && (found->supports_input || found->supports_rendering || found->supports_protocols)) {
                registered = true;
                fde_log(FDE_INFO, "Plugin '%s' registered via D-Bus", entry->d_name);
                break;
            }
        }
        if (!registered) {
            fde_log(FDE_INFO, "Plugin '%s' (PID %d) did not register in %ds; killing", entry->d_name, pid, timeout_sec);
            kill(pid, SIGTERM);
            // Удаляем temp (регистрация не произошла)
            plugin_list_remove(server, temp_plugin);
            free(temp_plugin->name);
            free(temp_plugin);
        }
    }

    closedir(dir);
    fde_log(FDE_INFO, "Loaded %d plugins from '%s'", wl_list_length(&server->plugins), config->plugins->dir);
    return launched_count > 0;
}