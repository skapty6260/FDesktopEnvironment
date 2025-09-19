#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <wlr/util/log.h>
#include <wlr/version.h>

#include <fde/compositor/compositor.h>
#include <fde/util/log.h>

static int exit_value = 0;
struct fde_server server = {0};
struct fde_debug debug = {0};

void fde_terminate(int exit_code) {
	
}

static const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"debug", no_argument, NULL, 'd'},
    {"config", required_argument, NULL, 'c'},
    {"validate", no_argument, NULL, 'C'},
    {"verbose", no_argument, NULL, 'V'}
};

static const char usage[] =
"Usage: fde [options] [command]\n"
	"\n"
	"  -h, --help             Show help message and quit.\n"
	"  -c, --config <config>  Specify a config file.\n"
	"  -C, --validate         Check the validity of the config file, then exit.\n"
	"  -d, --debug            Enables full logging, including debug information.\n"
	"  -v, --version          Show the version number and quit.\n"
	"  -V, --verbose          Enables more verbose logging.\n"
	// "      --get-socketpath   Gets the IPC socket path and prints it, then exits.\n"
	"\n"
;


static void log_env(void) {
	const char *log_vars[] = {
		"LD_LIBRARY_PATH",
		"LD_PRELOAD",
		"PATH",
		"FDE SOCKET",
	};
	for (size_t i = 0; i < sizeof(log_vars) / sizeof(char *); ++i) {
		char *value = getenv(log_vars[i]);
		fde_log(FDE_INFO, "%s=%s", log_vars[i], value != NULL ? value : "");
	}
}

static void log_file(FILE *f) {
	char *line = NULL;
	size_t line_size = 0;
	ssize_t nread;
	while ((nread = getline(&line, &line_size, f)) != -1) {
		if (line[nread - 1] == '\n') {
			line[nread - 1] = '\0';
		}
		fde_log(FDE_INFO, "%s", line);
	}
	free(line);
}

static void log_distro(void) {
	const char *paths[] = {
		"/etc/lsb-release",
		"/etc/os-release",
		"/etc/debian_version",
		"/etc/redhat-release",
		"/etc/gentoo-release",
	};
	for (size_t i = 0; i < sizeof(paths) / sizeof(char *); ++i) {
		FILE *f = fopen(paths[i], "r");
		if (f) {
			fde_log(FDE_INFO, "Contents of %s:", paths[i]);
			log_file(f);
			fclose(f);
		}
	}
}

static void log_kernel(void) {
	FILE *f = popen("uname -a", "r");
	if (!f) {
		fde_log(FDE_INFO, "Unable to determine kernel version");
		return;
	}
	log_file(f);
	pclose(f);
}

void enable_debug_flag(const char *flag) {
	if (strcmp(flag, "noatomic") == 0) {
		debug.noatomic = true;
	} else if (strcmp(flag, "txn-wait") == 0) {
		debug.txn_wait = true;
	} else if (strcmp(flag, "txn-timings") == 0) {
		debug.txn_timings = true;
	// } else if (has_prefix(flag, "txn-timeout=")) {
	// 	server.txn_timeout_ms = atoi(&flag[strlen("txn-timeout=")]);
	} else {
		fde_log(FDE_ERROR, "Unknown debug flag: %s", flag);
	}
}

static fde_log_importance_t convert_wlr_log_importance(enum wlr_log_importance importance) {
	switch (importance) {
	case WLR_ERROR:
		return FDE_ERROR;
	case WLR_INFO:
		return FDE_INFO;
	default:
		return FDE_DEBUG;
	}
}

static void handle_wlr_log(enum wlr_log_importance importance, const char *fmt, va_list args) {
	static char sway_fmt[1024];
	snprintf(sway_fmt, sizeof(sway_fmt), "[wlr] %s", fmt);
	_fde_vlog(convert_wlr_log_importance(importance), sway_fmt, args);
}

int main(int argc, char *argv[]) {
    static bool verbose = false, debug = false, validate = false;

    char *config_path = NULL;

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
		case 'D': // extended debug options
			enable_debug_flag(optarg);
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
	}

	if (debug) {
		fde_log_init(FDE_DEBUG, fde_terminate);
		wlr_log_init(WLR_DEBUG, handle_wlr_log);
	} else if (verbose) {
		fde_log_init(FDE_INFO, fde_terminate);
		wlr_log_init(WLR_INFO, handle_wlr_log);
	} else {
		fde_log_init(FDE_ERROR, fde_terminate);
		wlr_log_init(WLR_ERROR, handle_wlr_log);
	}

	fde_log(FDE_INFO, "FDE version " FDE_VERSION);
	fde_log(FDE_INFO, "wlroots version " WLR_VERSION_STR);
	log_kernel();
	log_distro();
	log_env();

	if (!server_init(&server)) {
		return 1;
	}

	if (!server_start(&server)) {
		fde_terminate(EXIT_FAILURE);
		goto shutdown;
	}

	server_run(&server);

shutdown:
	fde_log(FDE_INFO, "Shutting down FDE");

	return exit_value;
}