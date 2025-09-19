#include <fde/compositor/scene_graph.h>
#include <fde/compositor/tree/workspace.h>

// Что такое workspace_capacity и зачем он нужен?

// workspace_capacity — это размер выделенного массива указателей на рабочие пространства (workspace_t **workspaces). Он показывает, сколько элементов может вместить текущий выделенный блок памяти без необходимости перераспределения.

//     Когда вы добавляете новое рабочее пространство, если workspace_count (текущее количество) достигает workspace_capacity, нужно выделить новый, больший блок памяти (обычно с запасом, например, в 2 раза больше), скопировать туда старые элементы и освободить старый.
//     Это стандартный приём для динамических массивов в C, чтобы эффективно управлять памятью и минимизировать количество выделений.

// Без capacity вы не сможете понять, когда нужно расширять массив.

typedef struct root {
    scene_node_t *scene_root;

    workspace_t **workspaces;
    size_t workspace_count;
    size_t workspace_capacity;

    workspace_t *active_workspace;
} root_t;

root_t create_root(void);
void root_destroy(root_t *root);

void root_add_workspace(root_t *root, workspace_t *ws);
void root_remove_workspace(root_t *root, workspace_t *ws);

// Активировать рабочее пространство по id или указателю
bool root_activate_workspace_by_id(root_t *root, uint32_t id);
bool root_activate_workspace_by_ptr(root_t *root, workspace_t *ws);

workspace_t *root_get_workspace_by_id(root_t *root, uint32_t id);

// Обновить состояние сцены (например, применить транзакции, обновить рендер)
void root_update(root_t *root);