#include <fde/comp/compositor.h>
#include <fde/input/cursor.h>

#include <wlr/types/wlr_cursor.h>
#include <wlr/types/wlr_xcursor_manager.h>
#include <wlr/types/wlr_seat.h>

static void process_cursor_motion(fde_seat_t *seat, uint32_t time) {
	/* If the mode is non-passthrough, delegate to those functions. */
	// if (server->cursor_mode == TINYWL_CURSOR_MOVE) {
	// 	process_cursor_move(server);
	// 	return;
	// } else if (server->cursor_mode == TINYWL_CURSOR_RESIZE) {
	// 	process_cursor_resize(server);
	// 	return;
	// }

	/* Otherwise, find the toplevel under the pointer and send the event along. */
	wlr_cursor_set_xcursor(seat->cursor, seat->cursor_mgr, "default");
	
	// if (surface) {
	// 	/*
	// 	 * Send pointer enter and motion events.
	// 	 *
	// 	 * The enter event gives the surface "pointer focus", which is distinct
	// 	 * from keyboard focus. You get pointer focus by moving the pointer over
	// 	 * a window.
	// 	 *
	// 	 * Note that wlroots will avoid sending duplicate enter/motion events if
	// 	 * the surface has already has pointer focus or if the client is already
	// 	 * aware of the coordinates passed.
	// 	 */
		// wlr_seat_pointer_notify_enter(server->default_seat->wlr_seat, surface, sx, sy);
		// wlr_seat_pointer_notify_motion(server->default_seat->wlr_seat, time, sx, sy);
	// } else {
	// 	/* Clear pointer focus so future button events and such are not sent to
	// 	 * the last client to have the cursor over it. */
	// 	wlr_seat_pointer_clear_focus(seat);
	// }
}

void cursor_axis_handler(struct wl_listener *listener, void *data) {
    fde_seat_t *seat = wl_container_of(listener, seat, cursor_axis);
	struct wlr_pointer_axis_event *event = data;
	/* Notify the client with pointer focus of the axis event. */
	wlr_seat_pointer_notify_axis(seat->wlr_seat,
			event->time_msec, event->orientation, event->delta,
			event->delta_discrete, event->source, event->relative_direction);
};
void cursor_frame_handler(struct wl_listener *listener, void *data) {
	fde_seat_t *seat = wl_container_of(listener, seat, cursor_frame);
	wlr_seat_pointer_notify_frame(seat->wlr_seat);
};
void cursor_button_handler(struct wl_listener *listener, void *data) {
    struct wlr_pointer_button_event *event = data;
    /* Notify the client with pointer focus that a button press has occurred */
	wlr_seat_pointer_notify_button(server->default_seat->wlr_seat,
			event->time_msec, event->button, event->state);

};
void cursor_motion_handler(struct wl_listener *listener, void *data) {
    fde_seat_t *seat = wl_container_of(listener, seat, cursor_frame);
	struct wlr_pointer_motion_event *event = data;
	/* The cursor doesn't move unless we tell it to. The cursor automatically
	 * handles constraining the motion to the output layout, as well as any
	 * special configuration applied for the specific input device which
	 * generated the event. You can pass NULL for the device if you want to move
	 * the cursor around without any input. */
	wlr_cursor_move(seat->cursor, &event->pointer->base,
			event->delta_x, event->delta_y);
	process_cursor_motion(seat, event->time_msec);
};
void cursor_motion_absolute_handler(struct wl_listener *listener, void *data) {
	fde_seat_t *seat = wl_container_of(listener, seat, cursor_frame);
	struct wlr_pointer_motion_absolute_event *event = data;
	wlr_cursor_warp_absolute(seat->cursor, &event->pointer->base, event->x,
		event->y);
	process_cursor_motion(seat, event->time_msec);
};