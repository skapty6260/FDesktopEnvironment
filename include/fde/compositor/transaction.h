#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <fde/compositor/scene_graph.h>

// Структура транзакции — набор изменений, которые будут применены атомарно
typedef struct transaction {
    // Массив узлов, состояние которых изменилось в этой транзакции
    scene_node_t **modified_nodes;
    size_t modified_count;
    size_t modified_capacity;

    // Флаг, указывающий, была ли транзакция уже применена
    bool committed;
} transaction_t;

// Создать новую пустую транзакцию
transaction_t *transaction_create(void);

// Освободить ресурсы транзакции
void transaction_destroy(transaction_t *txn);

// Добавить узел в транзакцию (отметить, что он изменён)
void transaction_add_node(transaction_t *txn, scene_node_t *node);

// Установить позицию узла в транзакции (в pending_state)
void transaction_set_position(transaction_t *txn, scene_node_t *node, int32_t x, int32_t y);

// Установить размер узла в транзакции (в pending_state)
void transaction_set_size(transaction_t *txn, scene_node_t *node, int32_t width, int32_t height);

// Установить видимость узла в транзакции (в pending_state)
void transaction_set_visible(transaction_t *txn, scene_node_t *node, bool visible);

// Попытаться применить транзакцию: копирует pending_state в current_state для всех узлов
// Возвращает true, если успешно
bool transaction_commit(transaction_t *txn);

// Откатить транзакцию: сбросить pending_state в состояние current_state для всех узлов
void transaction_rollback(transaction_t *txn);

