#include <fde/comp/compositor.h>
#include <fde/utils/log.h>
#include <fde/comp/socket.h>

#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>  // mkdir, stat
#include <pwd.h>       // getpwuid для fallback UID
#include <errno.h>     // errno
#include <unistd.h>

// Получить XDG_RUNTIME_DIR (fallback: /tmp/runtime-$UID если не установлен).
char *get_xdg_runtime_dir(void) {
    const char *env = getenv("XDG_RUNTIME_DIR");
    if (env && env[0]) {
        fde_log(FDE_DEBUG, "Using XDG_RUNTIME_DIR from env: %s", env);
        return strdup(env);  // Copy для ownership.
    }

    // Fallback: Создать /tmp/xdg-runtime-$UID (как в spec; но лучше systemd).
    uid_t uid = getuid();
    char *fallback = malloc(64);  // Достаточно для /tmp/xdg-runtime-XXXXX.
    if (!fallback) return NULL;
    snprintf(fallback, 64, "/tmp/xdg-runtime-%u", uid);

    // Создать dir если нет (mode 0700, user-only).
    if (mkdir(fallback, 0700) == -1 && errno != EEXIST) {
        fde_log(FDE_ERROR, "Failed to create fallback XDG_RUNTIME_DIR %s: %s", fallback, strerror(errno));
        free(fallback);
        return NULL;
    }

    fde_log(FDE_DEBUG, "Using fallback XDG_RUNTIME_DIR: %s", fallback);
    setenv("XDG_RUNTIME_DIR", fallback, true);  // Установить для процесса.
    return fallback;
}

// Создать полный путь к сокету (XDG_RUNTIME_DIR + name).
char *make_socket_path(const char *runtime_dir, const char *name) {
    if (!runtime_dir || !name) return NULL;
    size_t len = strlen(runtime_dir) + strlen(name) + 2;  // / + \0.
    char *path = malloc(len);
    if (!path) return NULL;
    snprintf(path, len, "%s/%s", runtime_dir, name);
    return path;
}

// Обработать WAYLAND_SOCKET (nested: использовать существующий fd вместо new socket).
bool handle_wayland_socket_env(compositor_t *server) {
    const char *socket_env = getenv("WAYLAND_SOCKET");
    if (!socket_env) {
        fde_log(FDE_DEBUG, "No WAYLAND_SOCKET env; creating new socket");
        server->socket_fd = -1;  // No fd.
        return true;
    }

    // Parse fd (int from env).
    char *endptr;
    long fd_val = strtol(socket_env, &endptr, 10);
    if (*endptr != '\0' || fd_val < 0 || fd_val > INT_MAX) {
        fde_log(FDE_ERROR, "Invalid WAYLAND_SOCKET '%s': not a valid fd", socket_env);
        return false;
    }

    int fd = (int)fd_val;
    if (fcntl(fd, F_GETFD) == -1) {  // Check if fd valid.
        fde_log(FDE_ERROR, "WAYLAND_SOCKET fd %d invalid: %s", fd, strerror(errno));
        return false;
    }

    server->socket_fd = fd;
    fde_log(FDE_INFO, "Using existing WAYLAND_SOCKET fd %d (nested mode)", fd);

    // В wlroots: Backend auto-detects fd; но для display — используйте wl_display_add_socket_fd.
    // wl_display_add_socket_fd(server->wl_display, fd);  // Если нужно bind fd to display (wlroots handles in autocreate).
    return true;
}
