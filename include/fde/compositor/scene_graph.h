#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Forward declaration
struct scene_node;

typedef enum scene_node_type {
    SCENE_NODE_ROOT,
    SCENE_NODE_OUTPUT,
    SCENE_NODE_WORKSPACE,
    SCENE_NODE_CONTAINER
    // Should be extendable
    // Root -> all displays
    // Output -> single display that contains virtual workspaces
    // Workspace -> single workspace that contains windows
    // Container -> window
    // I should add new types for shell layers and scene background (wallpaper scene)
} scene_node_type_t;

// Состояние сцены (Двойная буферизация)
typedef struct scene_node_state {
    int32_t x, y;          // Позиция относительно родителя
    int32_t width, height; // Размеры
    bool visible;          // Видимость
    // Можно добавлять поля в зависимости от типа узла (Для окон добавить альфа канал условно и тд и тп йоу)
} scene_node_state_t;

typedef struct scene_node {
    scene_node_type_t type;

    struct scene_node *parent;
    struct scene_node **children;
    size_t children_count;
    size_t children_capacity;

    scene_node_state_t current_state; // Для рендера
    scene_node_state_t pending_state; // Для обновления

    bool state_dirty; // Флаг для изменений

    void *data; // Указатель на данные, специфичные для типа узла
} scene_node_t;

scene_node_t *scene_node_create(scene_node_type_t type); // Создание нового узла сцены указанного типа
void scene_node_destroy(scene_node_t *node); // Удаление узла сцены и всех его потомков
void scene_node_add_child(scene_node_t *parent, scene_node_t *child); // Добавление дочернего узла
void scene_node_remove_child(scene_node_t *parent, scene_node_t *child); // Удаление дочернего узла (без удаления самого узла)
void scene_node_commit(scene_node_t *node); // Обновление трансформаций и состояний (применение транзакций)

// Обход дерева сцены с вызовом callback для каждого узла
typedef void (*scene_node_visitor_t)(scene_node_t *node, void *user_data);
void scene_node_traverse(scene_node_t *root, scene_node_visitor_t visitor, void *user_data);

// Поиск узла по условию (callback возвращает true, если найден)
typedef bool (*scene_node_predicate_t)(scene_node_t *node, void *user_data);
scene_node_t *scene_node_find(scene_node_t *root, scene_node_predicate_t predicate, void *user_data);

// Операции scene_node_state (Только для pending, current не обновляется мануально)
void scene_node_set_position(scene_node_t *node, int32_t x, int32_t y); // Установка позиции в pending_state
void scene_node_set_size(scene_node_t *node, int32_t width, int32_t height); // Установка размера в pending_state
void scene_node_set_visible(scene_node_t *node, bool visible); // Установка видимости в pending_state
bool scene_node_is_dirty(const scene_node_t *node); // Проверка, изменилось ли состояние (pending != current)