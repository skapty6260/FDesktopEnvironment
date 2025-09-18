#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// #include "include/common/log.h"
#include <fde/common/log.h>

// #define FDE_VERSION = "1.01"

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

void enable_debug_flag(const char *flag) {
// 	if (strcmp(flag, "noatomic") == 0) {
// 		debug.noatomic = true;
// 	} else if (strcmp(flag, "txn-wait") == 0) {
// 		debug.txn_wait = true;
// 	} else if (strcmp(flag, "txn-timings") == 0) {
// 		debug.txn_timings = true;
// 	} else if (has_prefix(flag, "txn-timeout=")) {
// 		server.txn_timeout_ms = atoi(&flag[strlen("txn-timeout=")]);
// 	} else {
// 		fde_log(FDE_ERROR, "Unknown debug flag: %s", flag);
// 	}
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
			printf("fde version %s \n", "1.13");
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

    return 0;
}