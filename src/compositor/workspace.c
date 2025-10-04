#include <fde/comp/compositor.h>
#include <fde/comp/output.h>
#include <fde/comp/container.h>
#include <fde/comp/workspace.h>
#include <fde/utils/log.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-util.h>

#include <wlr/render/wlr_renderer.h>  // Для цветов (ARGB)
#include <wlr/types/wlr_scene.h>     // Для scene API

#define CALLOC_AND_CHECK(ptr, type, logmsg) \
    ptr = calloc(1, sizeof(type)); \
    if (!ptr) { \
      fde_log(FDE_ERROR, logmsg); \
      return; \
    };

void workspace_assign_to_output(workspace_t *ws, fde_output_t *output) {
    if (ws->output != NULL) {
        // Switch workspaces between outputs
        // if (output->active_ws == NULL) {
            
        // }
        // else {
        //     workspace_t prev_ws = ws->output->active_ws; 

        //     ws->output->active_ws = output->active_ws;
        //     output->active_ws = prev_ws;

        //     prev_ws = NULL;
        // }
        fde_log(FDE_ERROR, "Changed the output of a workspace that already has an output");
    }

    output->active_ws = ws;
    ws->output = output;

    // Создаём scene_tree для workspace и привязываем к output scene
    ws->scene_tree = wlr_scene_tree_create(&output->scene_output->scene->tree);
    if (!ws->scene_tree) {
        fde_log(FDE_ERROR, "Failed to create scene_tree for workspace %s", ws->name);
        return;
    }
    // Инициализируем background и container внутри workspace
    workspace_init_scene(ws);
}

void init_workspaces(
    struct wl_list *ws_list,
    char ws_names[MAX_NUM_WORKSPACES][MAX_WORKSPACE_NAME_LEN],
    compositor_t *server
) {
    fde_log(FDE_INFO, "Creating workspaces");

    wl_list_init(ws_list);

    char *name;
    workspace_t *ws;
    for (size_t i = 0; i < MAX_NUM_WORKSPACES; i++) {
        name = ws_names[i];
        if (!strlen(name)) {
            fde_log(FDE_DEBUG, "No more workspace names found");
            break;
        }

        CALLOC_AND_CHECK(ws, workspace_t, "Failed to create workspace instance");
        memcpy(ws->name, name, sizeof(char) * MAX_WORKSPACE_NAME_LEN);
        wl_list_insert(ws_list->prev, &ws->server_link);

        ws->server=server;
        
        ws->scene_tree = NULL;
        ws->background_node = NULL;
        ws->container_tree = NULL;

        fde_log(FDE_DEBUG, "Initialized workspace %s", ws->name);
    }
}

// Новая функция: Инициализация background и container в scene_tree workspace
void workspace_init_scene(workspace_t *ws) {
    if (!ws->scene_tree || !ws->output) {
        fde_log(FDE_ERROR, "Cannot init scene: no scene_tree or output for workspace %s", ws->name);
        return;
    }

    // Background: простой прямоугольник (leaf node, без детей). Размер — весь output (или workspace area)
    struct wlr_output *wlr_out = ws->output->wlr_output;
    int width = wlr_out->width;  // Или logical width с scale
    int height = wlr_out->height;
    float bg_color[4] = {255, 255, 255, 1};  // Чёрный (ARGB), настройте по умолчанию
    ws->background_node = wlr_scene_rect_create(ws->scene_tree, width, height,bg_color);
    if (!ws->background_node) {
        fde_log(FDE_ERROR, "Failed to create background_node for workspace %s", ws->name);
        return;
    }
    // Позиция background: (0,0) относительно workspace
    wlr_scene_node_set_position(&ws->background_node->node, 0, 0);

    // Container: subtree для окон (добавляется как ребёнок workspace_tree)
    ws->container_tree = wlr_scene_tree_create(ws->scene_tree);
    if (!ws->container_tree) {
        fde_log(FDE_ERROR, "Failed to create container_tree for workspace %s", ws->name);
        return;
    }
    // Позиция container: (0,0), но он будет выше background по z-order (добавлен позже)

    fde_log(FDE_INFO, "Scene initialized for workspace %s: background + container", ws->name);
}

// Новая функция: Добавление контейнера (окна/shell) в scene
void workspace_add_container(workspace_t *ws, fde_container_t *container) {
    if (!ws->container_tree || !container) {
        fde_log(FDE_ERROR, "Cannot add container: no container_tree or invalid container");
        return;
    }

    // Предполагаем, что fde_container_t имеет wlr_surface или wlr_xdg_surface
    // Создаём scene node для surface (адаптируйте под вашу структуру container)
    // Пример для xdg_toplevel: struct wlr_scene_tree *cont_tree = wlr_scene_xdg_surface_create(&container->xdg_surface->surface->scene_tree, ...); но упрощённо
    struct wlr_scene_surface *surface_node = wlr_scene_surface_create(ws->container_tree, container->surface);  // container->surface — ваш wlr_surface
    if (!surface_node) {
        fde_log(FDE_ERROR, "Failed to create scene_surface for container");
        return;
    }

    // Добавляем в wl_list containers
    wl_list_insert(&ws->containers, &container->link);  // Предполагаем, что в fde_container_t есть struct wl_list link;

    // Инициализируем позицию (e.g., по умолчанию в центре или по layout)
    wlr_scene_node_set_position(&surface_node->buffer->node, 100, 100);  // Пример

    // Обновляем layout (z-order, фокус)
    if (!ws->focused_container) {
        ws->focused_container = container;
        wlr_scene_node_raise_to_top(&surface_node->buffer->node);  // Поднимаем в топ
    }

    fde_log(FDE_DEBUG, "Added container to workspace %s scene", ws->name);
    workspace_update_layout(ws);  // Перерасполагаем все
}

// Новая функция: Удаление контейнера из scene
void workspace_remove_container(workspace_t *ws, fde_container_t *container) {
    // Найдите node контейнера (нужно хранить ссылку в fde_container_t, e.g., struct wlr_scene_node *scene_node;)
    if (container->scene_node) {  // Добавьте поле в fde_container_t
        wlr_scene_node_destroy(container->scene_node);
        container->scene_node = NULL;
    }

    wl_list_remove(&container->link);
    wl_list_init(&container->link);  // Reset

    // Обновляем фокус
    if (ws->focused_container == container) {
        ws->focused_container = NULL;  // Или следующий
    }
    if (ws->fullscreen_container == container) {
        ws->fullscreen_container = NULL;
    }

    workspace_update_layout(ws);
    fde_log(FDE_DEBUG, "Removed container from workspace %s", ws->name);
}

// Пример: Настройка цвета background
// void workspace_set_background_color(workspace_t *ws, uint32_t color) {
//     if (ws->background_node) {
//         ws->background_node->color = color;
//         wlr_scene_node_schedule_redraw(&ws->background_node->node);  // Обновляем damage
//     }
// }

// Пример: Обновление layout (перепозиционирование containers)
void workspace_update_layout(workspace_t *ws) {
    if (!ws->container_tree) return;

    // Итерация по containers и установка позиций (ваш tiling/floating logic)
    fde_container_t *cont;
    int x = 0, y = 0;  // Пример: stack layout
    wl_list_for_each(cont, &ws->containers, link) {
        if (cont->scene_node) {  // Поле в fde_container_t
            wlr_scene_node_set_position(cont->scene_node, x, y);
            x += cont->width;  // Адаптируйте под реальные размеры
        }
    }

    // Для fullscreen: wlr_scene_node_set_position(fullscreen->scene_node, 0, 0); и raise_to_top
    if (ws->fullscreen_container && ws->fullscreen_container->scene_node) {
        wlr_scene_node_raise_to_top(ws->fullscreen_container->scene_node);
    }

    // Damage: wlroots автоматически отслеживает, но можно явно wlr_scene_node_schedule_redraw(&ws->scene_tree->node);
    fde_log(FDE_DEBUG, "Updated layout for workspace %s", ws->name);
}
