#pragma once

#define MAX_NUM_WORKSPACES 50
#define MAX_WORKSPACE_NAME_LEN 80

#include <fde/comp/compositor.h>
#include <fde/comp/output.h>

typedef struct fde_output fde_output_t;
typedef struct compositor compositor_t;
typedef struct fde_container fde_container_t;

typedef struct fde_workspace {
    char name[100];
    
    fde_output_t *output;
    compositor_t *server;

    struct wl_list containers;
    fde_container_t *focused_container;
    fde_container_t *fullscreen_container;

    // Scene nodes для иерархии: root -> output -> workspace
    struct wlr_scene_tree *scene_tree;  // Корень для workspace (добавляется в output->scene_output->scene->tree)

    // Внутри workspace: background (leaf, без детей) и container (subtree для окон)
    struct wlr_scene_rect *background_node;  // Или кастомный node для фона (e.g., изображение/3D)
    struct wlr_scene_tree *container_tree;   // Subtree для всех containers (окон)

    struct wl_list server_link;
} workspace_t;  

void workspace_assign_to_output(workspace_t *ws, fde_output_t *output);
void init_workspaces(
    struct wl_list *ws_list,
    char ws_names[MAX_NUM_WORKSPACES][MAX_WORKSPACE_NAME_LEN],
    compositor_t *server
);

void workspace_init_scene(workspace_t *ws);
void workspace_add_container(workspace_t *ws, fde_container_t *container);  // Добавление контейнера в scene
void workspace_remove_container(workspace_t *ws, fde_container_t *container);  // Удаление
void workspace_set_background_color(workspace_t *ws, float color[4]);  // Пример: настройка фона
void workspace_update_layout(workspace_t *ws);  // Перерасположение containers в scene (позиции, z-order)