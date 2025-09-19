#include <stdint.h>

#include <fde/compositor/scene_graph.h>

typedef struct workspace {
    scene_node_t *scene_node;

    char *name;
    uint32_t id;

    double x, y;
    int width, height;
} workspace_t;

// Что должны делать воркспэйсы
// Применять лэйаут окон
// Хранить окна и передавать их другим воркспэйсам если нужно
// Инициализация и деструктор
// Хранить в себе layout окон

workspace_t *workspace_create(uint32_t id, char *name);

void workspace_destroy(workspace_t *ws); // При удалении должна перемещаться на другой воркспэйс (не забываем, не объебываемся)

void workspace_add_window(workspace_t *ws, scene_node_t *window_node);
void workspace_remove_window(workspace_t *ws, scene_node_t *window_node);
void workspace_move_window(workspace_t *ws_from, workspace_t *ws_to, scene_node_t *window_node);