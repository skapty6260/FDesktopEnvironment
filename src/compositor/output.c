#include <fde/comp/workspace.h>
#include <fde/comp/compositor.h>
#include <fde/comp/output.h>
#include <fde/utils/log.h>

#include <stdlib.h>
#include <wayland-server-core.h>
#include <wayland-util.h>
#include <wlr/types/wlr_output_layout.h>
#include <wlr/types/wlr_scene.h>

#define HANDLE_OUTPUT_EVENT(evt_name, evt, func) static void evt_name(struct wl_listener *list, void *data){\
    fde_output_t *output = wl_container_of(list, output, evt); \
    func(output, data); \
}

static void apply_output_mode(struct wlr_output *wlr_output, struct wlr_output_state *state) {
    // Set preferred mode
    // TODO: Made this configurable
    struct wlr_output_mode *mode = wlr_output_preferred_mode(wlr_output);
    if (mode != NULL) {
        wlr_output_state_set_mode(state, mode);
        // wlr_output_state_set_custom_mode() to use mode from config
    }
}

// TODO: Add plugins event init to add elements to scene
static void start_using_output(fde_output_t *output) {
    compositor_t *server = output->server;
    wl_list_insert(&server->outputs, &output->link);

    // Init workspaces and add scene;
    workspace_t *current_ws;
    wl_list_for_each(current_ws, &server->workspaces, server_link) {
        if (current_ws->output == NULL) {
            fde_log(FDE_INFO, "Assigning new output workspace %s", current_ws->name);
            workspace_assign_to_output(current_ws, output);
            break;
        }
    }

    wlr_output_layout_add_auto(server->output_layout, output->wlr_output);
}

void frame(fde_output_t *output, void *data) {
    struct wlr_scene_output *scene_output = output->scene_output;
    if (!scene_output) {
        fde_log(FDE_ERROR, "No scene_output for output %s", output->wlr_output->name);
        return;
    }

    wlr_scene_output_commit(scene_output, NULL);

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    wlr_scene_output_send_frame_done(scene_output, &now);
}
void request_state(fde_output_t *output, void *data) {
    const struct wlr_output_event_request_state *event = data;
	wlr_output_commit_state(output->wlr_output, event->state);
}
void destroy(fde_output_t *output, void *data) {
    wl_list_remove(&output->frame.link);
	wl_list_remove(&output->request_state.link);
	wl_list_remove(&output->destroy.link);
	wl_list_remove(&output->link);
	free(output);
}

HANDLE_OUTPUT_EVENT(output_frame, frame, frame);
HANDLE_OUTPUT_EVENT(output_request_state, request_state, request_state);
HANDLE_OUTPUT_EVENT(output_destroy, destroy, destroy)

void server_new_output(struct wl_listener *listener, void *data) {
    compositor_t *server = wl_container_of(listener, server, new_output);
    struct wlr_output *wlr_output = data;

    fde_log(FDE_INFO,
        "New output appeared with name %s, make %s, model %s, serial %s",
        wlr_output->name,
        wlr_output->make,
        wlr_output->model,
        wlr_output->serial
    );
    
    wlr_output_init_render(wlr_output, server->allocator, server->renderer);
    
    struct wlr_output_state state;
    wlr_output_state_init(&state);
    wlr_output_state_set_enabled(&state, true);

    apply_output_mode(wlr_output, &state);

    wlr_output_commit_state(wlr_output, &state);
    wlr_output_state_finish(&state);

    fde_output_t *output = calloc(1, sizeof(fde_output_t));
    output->wlr_output = wlr_output;
    output->server = server;
    
    /* Sets up a listener for the frame event. */
	output->frame.notify = output_frame;
	wl_signal_add(&wlr_output->events.frame, &output->frame);

	/* Sets up a listener for the state request event. */
	output->request_state.notify = output_request_state;
	wl_signal_add(&wlr_output->events.request_state, &output->request_state);

	/* Sets up a listener for the destroy event. */
	output->destroy.notify = output_destroy;
	wl_signal_add(&wlr_output->events.destroy, &output->destroy);

    // Узел сцены для мониторов. Внутри: workspaces->background, containers
    output->scene_output = wlr_scene_output_create(server->scene, output->wlr_output);

    start_using_output(output);
}