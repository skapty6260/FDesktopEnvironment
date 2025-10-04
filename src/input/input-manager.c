#include <fde/utils/log.h>
#include <fde/comp/compositor.h>
#include <fde/input/input-manager.h>

#include <wayland-util.h>

#include <wlr/types/wlr_input_device.h>
#include <wlr/types/wlr_seat.h>
#include <wlr/types/wlr_cursor.h>

// static void configure_input_device(struct wlr_input_device *device, struct ) {}

static void server_new_pointer(compositor_t *server, struct wlr_input_device *device) {
    wlr_cursor_set_xcursor(server->default_seat->cursor, server->default_seat->cursor_mgr, "default");
}

void server_new_input(struct wl_listener *listener, void *data) {
    compositor_t *server = wl_container_of(listener, server, new_input);
    struct wlr_input_device *device = data;
	switch (device->type) {
	case WLR_INPUT_DEVICE_KEYBOARD:
        // viv_seat_create_new_keyboard(viv_server_get_default_seat(server), device);
        fde_log(FDE_INFO, "New keyboard detected");
		break;
	case WLR_INPUT_DEVICE_POINTER:
        fde_log(FDE_INFO, "New pointer (mouse) detected");
		server_new_pointer(server, device);
		break;
	default:
        fde_log(FDE_ERROR, "Received an unrecognised/unhandled new input, type %d", device->type);
		break;
	}

    wlr_log(WLR_INFO, "New input device with name \"%s\"", device->name);
    
    // Configure the new device, e.g. applying libinput config options

    fde_seat_t *seat;
    wl_list_for_each(seat, &server->seats, server_link) {
        uint32_t caps = WL_SEAT_CAPABILITY_POINTER;
        if (!wl_list_empty(&seat->keyboards)) {
            caps |= WL_SEAT_CAPABILITY_KEYBOARD;
        }
        wlr_seat_set_capabilities(seat->wlr_seat, caps);
    }
}