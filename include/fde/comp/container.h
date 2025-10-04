#pragma once

#include <wayland-util.h>

enum container_type {
    CONTAINER_TYPE_UNKNOWN,
    CONTAINER_TYPE_XDG_SHELL,
#ifdef XWAYLAND
    CONTAINER_TYPE_XWAYLAND
#endif
};

typedef struct fde_container {
    enum container_type type;
    
    struct wlr_scene_node *scene_node;

    struct wl_list link;  // Для включения в списки (например, workspace->containers)
    // Основные данные окна
    struct wlr_surface *surface;          // Поверхность Wayland клиента
    struct wlr_scene_surface *scene_surface;  // Узел сцены для рендеринга surface
    // Позиция и размер контейнера (логические пиксели)
    int x, y;
    int width, height;

    union {
        struct wlr_xdg_surface *xdg_surface;
        struct wlr_xwayland_surface *xwayland_surface;
    };
    
} fde_container_t;