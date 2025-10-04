#include <fde/comp/compositor.h>
#include <fde/comp/output.h>
#include <fde/comp/workspace.h>
#include <fde/utils/log.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-util.h>

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
        // wl_list_init(&workspace->views);
        // wl_list_insert(workspaces_list->prev, &workspace->server_link);

        // workspace->server = server;

        // // Set up layouts for the new workspace
        // init_layouts(&workspace->layouts, layouts);
        // struct viv_layout *active_layout = wl_container_of(workspace->layouts.next, active_layout, workspace_link);
        // workspace->active_layout = active_layout;
    }
}