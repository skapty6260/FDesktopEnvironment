#pragma once

#include <stdbool.h>

struct cli_args {
    char *config_path;
    bool verbose;
    bool validate;
    bool debug;
};

struct cli_args parse_cli_args(int argc, char *argv[]);