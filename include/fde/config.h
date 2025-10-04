#pragma once

#include <fde/comp/workspace.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef enum {
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_INT,
    TYPE_ARRAY
} config_value_type_t;

typedef struct {
    const char *key_name;
    config_value_type_t type;
    size_t offset; 
} config_key_desc_t;

typedef struct {
    const char *section_name;
    config_key_desc_t *keys;
    size_t num_keys;
} config_section_desc_t;

struct plugins {
    char *dir;
};

struct hotreload {
    bool enabled;
    int scan_interval;    
};

struct workspaces {
    char list[MAX_NUM_WORKSPACES][MAX_WORKSPACE_NAME_LEN];
};

struct fde_config {
    bool active; bool validating;

    struct plugins plugins;
    struct hotreload hr;
    struct workspaces workspaces;
};

// Singleton
extern struct fde_config *config;
extern struct fde_config default_conf;

bool load_config(const char *path, struct fde_config *config);
bool read_config(FILE *file, struct fde_config *config);
void free_config(struct fde_config *config);