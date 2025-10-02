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

static int exit_value = 0;
static bool terminate_request = false;
struct fde_config *config = {0};
compositor_t *server = {0};

// static char *str_concat(const char *str1, const char *str2) {  // Helper for safe concat (malloc).
//     if (!str1 || !str2) return NULL;
//     size_t len1 = strlen(str1), len2 = strlen(str2);
//     char *result = malloc(len1 + len2 + 1);
//     if (!result) return NULL;
//     snprintf(result, len1 + len2 + 1, "%s%s", str1, str2);
//     return result;
// }

// static char *get_xdg_config_home(void) {
//     char *xdg_config_home = getenv("XDG_CONFIG_HOME");
//     if (xdg_config_home && *xdg_config_home != '\0') {
//         fde_log(FDE_DEBUG, "Using XDG_CONFIG_HOME: %s", xdg_config_home);
//         return str_concat(xdg_config_home, "/fde/config.ini");  // Safe alloc.
//     }
//     char *home_dir = getenv("HOME");
//     if (home_dir && *home_dir != '\0') {
//         fde_log(FDE_DEBUG, "XDG_CONFIG_HOME not set. Using default: %s/.config/fde/config.ini", home_dir);
//         char *config_dir = str_concat(home_dir, "/.config/fde/");
//         if (!config_dir) return NULL;
//         char *full_path = str_concat(config_dir, "config.ini");
//         free(config_dir);
//         return full_path;
//     }
//     fde_log(FDE_ERROR, "XDG_CONFIG_HOME and HOME not set. Cannot determine config directory.");
//     return NULL;
// }

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

    // if (!getenv("XDG_RUNTIME_DIR")) {
	// 	fprintf(stderr, "XDG_RUNTIME_DIR is not set. Aborting.\n");
    //     exit(EXIT_FAILURE);
	// }

    fde_log(FDE_INFO, "FDE VERSION: " FDE_VERSION);
    fde_log(FDE_INFO, "WLROOTS VERSION: " WLR_VERSION_STR);

    // Config
    CALLOC_AND_CHECK(config, struct fde_config, free(parsed_args.config_path), "Failed to allocate config.", true);
    bool config_loaded = load_config(parsed_args.config_path, config);
    if (parsed_args.validate) {
        return config_loaded ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    if (!config_loaded) {
        fde_log(FDE_ERROR, "Failed to load config.");
        terminate(EXIT_FAILURE);
        goto shutdown;
    }

    // Compositor
    CALLOC_AND_CHECK(server, compositor_t, terminate(EXIT_FAILURE); goto shutdown, "Failed to create compositor server", false);
    if (!comp_init(server)) {
        return 1;
    }
    if (!comp_start(server)) {
        terminate(EXIT_FAILURE);
        goto shutdown;
    }

    // DBus
    if (!init_dbus(server)) {
        fde_log(FDE_ERROR, "Failed to init D-Bus");
        terminate(-1); 
        goto shutdown;
    }

    int dbus_fd = -1;
    dbus_bool_t fd_result = dbus_connection_get_unix_fd(server->dbus_conn, &dbus_fd);
    if (!fd_result || dbus_fd < 0) {
        fde_log(FDE_ERROR, "Failed to get D-Bus fd: result=%d, fd=%d", fd_result, dbus_fd);
        cleanup_dbus(server);
        goto shutdown;
    }
    fde_log(FDE_DEBUG, "D-Bus fd obtained: %d", dbus_fd);
    // Добавление fd в Wayland event loop (full mask)
    server->dbus_source = wl_event_loop_add_fd(
        server->wl_event_loop,
        dbus_fd,
        WL_EVENT_READABLE | WL_EVENT_WRITABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR,
        dbus_fd_handler,  // Из dbus.c
        server
    );
    if (!server->dbus_source) {
        fde_log(FDE_ERROR, "Failed to add D-Bus fd (%d) to event loop", dbus_fd);
        cleanup_dbus(server);
        goto shutdown;
    }
    fde_log(FDE_DEBUG, "D-Bus fd (%d) added to Wayland event loop", dbus_fd);

    if (!load_plugins_from_dir(server, config)) {
        fde_log(FDE_ERROR, "Failed to load plugins.");
    }
  
    comp_run(server);

shutdown:
    fde_log(FDE_INFO, "Shutting down fde");
    comp_destroy(server, config, parsed_args.config_path);  // Всё в одном вызове!
    return exit_value;
}