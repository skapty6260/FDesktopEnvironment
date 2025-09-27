// Мне нужно сделать гибкий композитор, у которого могут изменяться операции ввода, добавляться протоколы, кастомные клиенты к серверу, изменяться рендеринг и сцена
// Ключевые моменты - Стркутуры для модулей взаимодействия
// Реализация создания композитора и инициализации модульной структуры

#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fde/utils/log.h>
#include <fde/compositor.h>
#include <fde/plugin-system.h>
#include <fde/config.h>
#include <fde/dbus.h>

#include <wayland-server.h>
#include <wlr/util/log.h>
#include <wlr/version.h>

static int exit_value = 0;
static bool terminate_request = false;
struct fde_config *config = {0};
compositor_t *server = {0};

const char usage[] =
"Usage: fde [options] [command]\n"
	"\n"
	"  -h, --help             Show help message and quit.\n"
	"  -c, --config <config>  Specify a config file.\n"
	"  -C, --validate         Check the validity of the config file, then exit.\n"
	"  -d, --debug            Enables full logging, including debug information.\n"
	"  -v, --version          Show the version number and quit.\n"
	"  -V, --verbose          Enables more verbose logging.\n"
	"\n"
;

const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"debug", no_argument, NULL, 'd'},
    {"config", required_argument, NULL, 'c'},
    {"validate", no_argument, NULL, 'C'},
    {"verbose", no_argument, NULL, 'V'}
};

static char *str_concat(const char *str1, const char *str2) {  // Helper for safe concat (malloc).
    if (!str1 || !str2) return NULL;
    size_t len1 = strlen(str1), len2 = strlen(str2);
    char *result = malloc(len1 + len2 + 1);
    if (!result) return NULL;
    snprintf(result, len1 + len2 + 1, "%s%s", str1, str2);
    return result;
}

static char *get_xdg_config_home(void) {
    char *xdg_config_home = getenv("XDG_CONFIG_HOME");
    if (xdg_config_home && *xdg_config_home != '\0') {
        fde_log(FDE_DEBUG, "Using XDG_CONFIG_HOME: %s", xdg_config_home);
        return str_concat(xdg_config_home, "/fde/config.ini");  // Safe alloc.
    }
    char *home_dir = getenv("HOME");
    if (home_dir && *home_dir != '\0') {
        fde_log(FDE_DEBUG, "XDG_CONFIG_HOME not set. Using default: %s/.config/fde/config.ini", home_dir);
        char *config_dir = str_concat(home_dir, "/.config/fde/");
        if (!config_dir) return NULL;
        char *full_path = str_concat(config_dir, "config.ini");
        free(config_dir);
        return full_path;
    }
    fde_log(FDE_ERROR, "XDG_CONFIG_HOME and HOME not set. Cannot determine config directory.");
    return NULL;
}

void terminate(int exit_code) {
    terminate_request = true;
    exit_value = exit_code;
    // Should call shutdown event
    // wl_display_terminate(server.wl_display);
}

int main(int argc, char *argv[]) {
    static bool verbose = false, debug = false, validate = false;  
    char *config_path = get_xdg_config_home();

    // Args parser
    int c;
    while (1) {
        int option_index = 0;
        c = getopt_long(argc, argv, "hCdD:vVc:", long_options, &option_index);
        
        if (c == -1) {
            break;
        }
        
        switch (c) {
        case 'h': // help
            printf("%s", usage);
            exit(EXIT_SUCCESS);
            break;
        case 'c': // config
			free(config_path);
			config_path = strdup(optarg);
			break;
        case 'C': // validate
			validate = true;
			break;
        case 'd': // debug
			debug = true;
			break;
        case 'v': // version
			printf("fde version %s \n", FDE_VERSION);
			exit(EXIT_SUCCESS);
			break;
        case 'V': // verbose
			verbose = true;
			break;
        default:
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
        }
    };

    // Log level
    if (debug) {
		fde_log_init(FDE_DEBUG, terminate);
		wlr_log_init(WLR_DEBUG, handle_wlr_log);
	} else if (verbose) {
		fde_log_init(FDE_INFO, terminate);
		wlr_log_init(WLR_INFO, handle_wlr_log);
	} else {
		fde_log_init(FDE_ERROR, terminate);
		wlr_log_init(WLR_ERROR, handle_wlr_log);
	}

    if (!getenv("XDG_RUNTIME_DIR")) {
		fprintf(stderr, "XDG_RUNTIME_DIR is not set. Aborting.\n");
        free(config_path);
        exit(EXIT_FAILURE);
	}

    fde_log(FDE_INFO, "FDE VERSION: " FDE_VERSION);
    fde_log(FDE_INFO, "WLROOTS VERSION: " WLR_VERSION_STR);

    config = calloc(1, sizeof(struct fde_config));
    if (!config) {
        fde_log(FDE_ERROR, "Cannot alloc config");
        free(config_path);
        return EXIT_FAILURE;
    }

    bool config_valid = false;
    if (validate) {
        config_valid = load_config(config_path, config);
        free_config(config);
        free(config);
        free(config_path);

        if (config_valid) {
            printf("Config is valid.");
            return EXIT_SUCCESS;
        } else {
            printf("Config is invalid.");
            return EXIT_FAILURE;
        }
    }

    if (!load_config(config_path, config)) {
        fde_log(FDE_ERROR, "Failed to load config.");
        terminate(EXIT_FAILURE);
        goto shutdown;
    }

    server = calloc(1, sizeof(compositor_t));
    if (!server) {
        fde_log(FDE_ERROR, "Failed to create compositor.");
        terminate(EXIT_FAILURE);
        goto shutdown;
    }

    if (!comp_init(server)) {
        return 1;
    }

    if (!comp_start(server)) {
        terminate(EXIT_FAILURE);
        goto shutdown;
    }

    // Start ipc and plugins
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
        return -1;  // Или обработайте gracefully (e.g., без IPC)
    }
    fde_log(FDE_DEBUG, "D-Bus fd obtained: %d", dbus_fd);
    // Добавление fd в Wayland event loop
    struct wl_event_loop *loop = wl_display_get_event_loop(server->wl_display);  // Или server->wl_event_loop, если сохранили
    server->dbus_source = wl_event_loop_add_fd(  // Добавьте поле wl_event_source *dbus_source в compositor_t
        loop,
        dbus_fd,  // Теперь fd готов (int, не указатель)
        WL_EVENT_READABLE | WL_EVENT_WRITABLE | WL_EVENT_HANGUP | WL_EVENT_ERROR,  // Полная маска для robustness
        dbus_fd_handler,  // Ваш callback из предыдущего сообщения
        server  // user_data
    );
    if (!server->dbus_source) {
        fde_log(FDE_ERROR, "Failed to add D-Bus fd (%d) to event loop", dbus_fd);
        cleanup_dbus(server);
        return -1;
    }
    fde_log(FDE_INFO, "D-Bus fd (%d) added to Wayland event loop", dbus_fd);

    fde_log(FDE_INFO, "Starting FDE compositor...");
    wl_display_run(server->wl_display);

shutdown:
    fde_log(FDE_INFO, "Shutting down fde");

    wl_event_source_remove(server->dbus_source);
    cleanup_dbus(server);
    // cleanup_compositor(server);  // Ваша функция: wl_display_destroy, etc.
    free(server);

    if (config) {
        free(config_path);
        free_config(config);
    }

    return exit_value;
}