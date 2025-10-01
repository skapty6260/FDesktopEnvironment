#pragma once

#include <fde/comp/compositor.h>

char *get_xdg_runtime_dir(void);
char *make_socket_path(const char *runtime_dir, const char *name);
bool handle_wayland_socket_env(compositor_t *server);