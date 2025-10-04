#pragma once

#define MAX_NUM_WORKSPACES 50
#define MAX_WORKSPACE_NAME_LEN 80

#include <fde/comp/compositor.h>
#include <fde/comp/output.h>

typedef struct fde_output fde_output_t;
typedef struct compositor compositor_t;

typedef struct fde_workspace {
    char name[100];

    fde_output_t *output;

    compositor_t *server;

    struct wl_list server_link;
} workspace_t;  

void workspace_assign_to_output(workspace_t *ws, fde_output_t *output);
void init_workspaces(
    struct wl_list *ws_list,
    char ws_names[MAX_NUM_WORKSPACES][MAX_WORKSPACE_NAME_LEN],
    compositor_t *server
);