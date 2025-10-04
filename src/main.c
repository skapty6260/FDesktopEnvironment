// Мне нужно сделать гибкий композитор, у которого могут изменяться операции ввода, добавляться протоколы, кастомные клиенты к серверу, изменяться рендеринг и сцена
// Ключевые моменты - Стркутуры для модулей взаимодействия
// Реализация создания композитора и инициализации модульной структуры

#include <fde/utils/cli.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <fde/utils/log.h>
#include <fde/comp/compositor.h>
#include <fde/plugin-system.h>
#include <fde/config.h>
#include <fde/dbus.h>

#include <wayland-server-core.h>
#include <wayland-server.h>
#include <wlr/util/log.h>
#include <wlr/version.h>

#define CALLOC_AND_CHECK(ptr, type, destroy_fn, logmsg, free_ptr) \
    ptr = calloc(1, sizeof(type)); \
    if (!ptr) { \
      fde_log(FDE_ERROR, logmsg); \
      destroy_fn; \
      return EXIT_FAILURE; \
    };

#define MINIMIZE_CHECK(fn, cb) \
    if (fn) {   \
        cb \
    } \

static int exit_value = 0;
static bool terminate_request = false;
struct fde_config *config = {0};
compositor_t *server = {0};

void terminate(int exit_code) {
    terminate_request = true;
    exit_value = exit_code;
}

int main(int argc, char *argv[]) {
    struct cli_args parsed_args = parse_cli_args(argc, argv);

    if (parsed_args.debug) {
		fde_log_init(FDE_DEBUG, terminate);
		wlr_log_init(WLR_DEBUG, handle_wlr_log);
	} else if (parsed_args.verbose) {
		fde_log_init(FDE_INFO, terminate);
		wlr_log_init(WLR_INFO, handle_wlr_log);
	} else {
		fde_log_init(FDE_ERROR, terminate);
		wlr_log_init(WLR_ERROR, handle_wlr_log);
	}

    MINIMIZE_CHECK(!getenv("XDG_RUNTIME_DIR"), fprintf(stderr, "XDG_RUNTIME_DIR is not set. Aborting.\n"); exit(EXIT_FAILURE););

    fde_log(FDE_INFO, "FDE VERSION: " FDE_VERSION);
    fde_log(FDE_INFO, "WLROOTS VERSION: " WLR_VERSION_STR);

    // Config
    CALLOC_AND_CHECK(config, struct fde_config, free(parsed_args.config_path), "Failed to allocate config.", true);
    bool config_loaded = load_config(parsed_args.config_path, config);
    if (parsed_args.validate) {
        return config_loaded ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    MINIMIZE_CHECK(!config_loaded, fde_log(FDE_ERROR, "Failed to load config."); terminate(EXIT_FAILURE); goto shutdown;);

    // Compositor
    CALLOC_AND_CHECK(server, compositor_t, terminate(EXIT_FAILURE); goto shutdown, "Failed to create compositor server", false);
    MINIMIZE_CHECK(!comp_init(server), return 1;);
    MINIMIZE_CHECK(!comp_start(server), terminate(EXIT_FAILURE);goto shutdown;);

    server->socket = wl_display_add_socket_auto(server->wl_display);
	if (!server->socket) {
		wlr_backend_destroy(server->backend);
		return 1;
	}

    // Set the WAYLAND_DISPLAY environment variable, so that clients know how to connect
    // to our server
	setenv("WAYLAND_DISPLAY", server->socket, true);

    // Set up env vars to encourage applications to use wayland if possible
    setenv("QT_QPA_PLATFORM", "wayland", true);
    setenv("MOZ_ENABLE_WAYLAND", "1", true);

    // DBus
    MINIMIZE_CHECK(!init_dbus(server), fde_log(FDE_ERROR, "Failed to init D-Bus");terminate(EXIT_FAILURE);goto shutdown;);

    int dbus_fd = -1;
    dbus_bool_t fd_result = dbus_connection_get_unix_fd(server->dbus_conn, &dbus_fd);
    MINIMIZE_CHECK(!fd_result || dbus_fd < 0, fde_log(FDE_ERROR, "Failed to get D-Bus fd: result=%d, fd=%d", fd_result, dbus_fd);goto shutdown;);
    fde_log(FDE_DEBUG, "D-Bus fd obtained: %d", dbus_fd);

    // Добавление fd в Wayland event loop (full mask)
    server->dbus_source = wl_event_loop_add_fd(
        server->wl_event_loop,
        dbus_fd,
        WL_EVENT_READABLE | WL_EVENT_WRITABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR,
        dbus_fd_handler,  // Из dbus.c
        server
    );
    MINIMIZE_CHECK(!server->dbus_source, fde_log(FDE_ERROR, "Failed to add D-Bus fd (%d) to event loop", dbus_fd); goto shutdown;);

    fde_log(FDE_DEBUG, "D-Bus fd (%d) added to Wayland event loop", dbus_fd);
    
    // Plugins
    MINIMIZE_CHECK(!load_plugins_from_dir(server, config), fde_log(FDE_ERROR, "Failed to load plugins."););
  
    comp_run(server);

shutdown:
    fde_log(FDE_INFO, "Shutting down fde");
    comp_destroy(server, config, parsed_args.config_path);  // Всё в одном вызове!
    return exit_value;
}