#pragma once

#include <fde/comp/workspace.h>
#include <stdbool.h>
#include <stdio.h>

struct plugins {
    char *dir;
};

struct hotreload {
    bool enabled;
    int scan_interval;    
};

struct fde_config {
    bool active; bool validating;

    struct plugins *plugins;
    struct hotreload *hr;

    char workspaces[MAX_NUM_WORKSPACES][MAX_WORKSPACE_NAME_LEN];
};

// Singleton
extern struct fde_config *config;

bool load_config(const char *path, struct fde_config *config);
bool read_config(FILE *file, struct fde_config *config);
void free_config(struct fde_config *config);