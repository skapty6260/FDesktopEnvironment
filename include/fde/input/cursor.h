#pragma once

#include <wayland-server.h>

enum cursor_mode {
    CURSOR_PASSTHROUGH,
    CURSOR_MOVE,
    CURSOR_RESIZE
};

enum cursor_buttons {
    LEFT_BUTTON = 272,
    MIDDLE_BUTTON = 273,
    RIGHT_BUTTON = 274
};

void cursor_axis_handler(struct wl_listener *listener, void *data);
void cursor_frame_handler(struct wl_listener *listener, void *data);
void cursor_button_handler(struct wl_listener *listener, void *data);
void cursor_motion_handler(struct wl_listener *listener, void *data);
void cursor_motion_absolute_handler(struct wl_listener *listener, void *data);