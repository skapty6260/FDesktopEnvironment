#include <fde/utils/log.h>
#include <fde/input/seat.h>

#include <stdlib.h>

#include <wayland-util.h>
#include <wlr/types/wlr_seat.h>

fde_seat_t *create_seat(compositor_t *server, char *name) {
    fde_seat_t *seat = calloc(1, sizeof(fde_seat_t));
    if (!seat) {
        fde_log(FDE_ERROR, "Unable to create fde seat.");
        return NULL;
    }

    struct wlr_seat *wlr_seat = wlr_seat_create(server->wl_display, name); 
    seat->wlr_seat = wlr_seat;

    wl_list_init(&seat->server_link);
    wl_list_insert(server->seats.prev, &seat->server_link);

    wl_list_init(&seat->keyboards);

    return seat;
};