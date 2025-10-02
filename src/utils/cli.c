#include <fde/utils/cli.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define ALL_OPTIONS_CONSUMED -1
#define OPTION_PARSE_FAILURE '?'

const struct option long_options[] = {
    {"help", no_argument, NULL, 'h'},
    {"version", no_argument, NULL, 'v'},
    {"debug", no_argument, NULL, 'd'},
    {"config", required_argument, NULL, 'c'},
    {"validate", no_argument, NULL, 'C'},
    {"verbose", no_argument, NULL, 'V'}
};

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

static void handle_set_config_path(struct cli_args *args) {
    args->config_path = optarg;
}

struct cli_args parse_cli_args(int argc, char *argv[]) {
    int option_result;

    struct cli_args parsed_args = {0, false, false, false};

    while(true) {
        int option_index = 0;
        option_result = getopt_long(argc, argv, "hCdD:vVc:", long_options, &option_index);
        
        if (option_result == ALL_OPTIONS_CONSUMED) {
            break;
        }

        switch (option_result) {
        case 'h': // help
            printf("%s", usage);
            exit(EXIT_SUCCESS);
            break;
        case 'c': // config
			handle_set_config_path(&parsed_args);
			break;
        case 'C': // validate
			parsed_args.validate = true;
			break;
        case 'd': // debug
			parsed_args.debug = true;
			break;
        case 'v': // version
			printf("fde version %s \n", FDE_VERSION);
			exit(EXIT_SUCCESS);
			break;
        case 'V': // verbose
			parsed_args.verbose = true;
			break;
        case OPTION_PARSE_FAILURE:
            exit(EXIT_FAILURE);
            break;
        default:
			fprintf(stderr, "%s", usage);
			exit(EXIT_FAILURE);
        }
    }

    return parsed_args;
};