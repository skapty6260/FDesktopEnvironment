#include <stdio.h>
#include <string.h>
#include <stdlib.h>
// #include <ctype.h>    // isspace для trim
#include <unistd.h>   // getuid для ~
#include <pwd.h>      // getpwuid
#include <sys/stat.h> // Для load_config
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <fde/utils/log.h>
#include <fde/utils/config_helpers.h>
#include <fde/config.h>


// TODO: Переделать создание конфига если файл не найден в load_config. Что-то придумать с гитом или файлами
// TODO: Сделать поддержку вложенных плагинов: основной config.ini, в котором могут быть include(path). Например отдельные конфиги для каждого плагина, отдельный для input, keybinds, windows, etc.

struct fde_config default_conf = {
    .plugins = {
        .dir = "~/.config/fde/plugins/"
    },
    .hr = {
        .enabled = true,
        .scan_interval = 0
    },
    .workspaces = {
        .list = { "main", 2, 3, 4, 5, 6, 7, 8, 9 }
    }
};

// Initialize config with defaults
static void init_config_defaults(struct fde_config *config) {
    config->active = false;
    config->validating = true;

    // Initialize plugins.dir with default string
    config->plugins.dir = strdup(default_conf.plugins.dir ? default_conf.plugins.dir : "~/.config/fde/plugins/");
    config->hr.enabled = default_conf.hr.enabled;
    config->hr.scan_interval = default_conf.hr.scan_interval;

    // Initialize workspaces list with default names
    for (int i = 0; i < MAX_NUM_WORKSPACES; i++) {
        config->workspaces.list[i][0] = '\0';
    }
    strncpy(config->workspaces.list[0], "main", MAX_WORKSPACE_NAME_LEN - 1);
    config->workspaces.list[0][MAX_WORKSPACE_NAME_LEN - 1] = '\0';
}

// Parse workspaces section (special case)
static void parse_workspaces_section(const char *key, const char *value, struct fde_config *config, bool validating, int line_num) {
    // For example, keys like ws1, ws2, ... or just "list" with comma separated values
    // Here we support keys ws1..wsN for simplicity
    if (strncmp(key, "ws", 2) == 0) {
        int idx = atoi(key + 2) - 1;  // ws1 -> index 0
        if (idx >= 0 && idx < MAX_NUM_WORKSPACES) {
            strncpy(config->workspaces.list[idx], value, MAX_WORKSPACE_NAME_LEN - 1);
            config->workspaces.list[idx][MAX_WORKSPACE_NAME_LEN - 1] = '\0';
        } else if (validating) {
            fprintf(stderr, "Line %d: Workspace index out of range: %s\n", line_num, key);
        }
    } else if (strcmp(key, "list") == 0) {
        // Parse comma separated list
        char *copy = strdup(value);
        if (!copy) return;
        char *token = strtok(copy, ",");
        int idx = 0;
        while (token && idx < MAX_NUM_WORKSPACES) {
            char *trimmed = trim(token);
            strncpy(config->workspaces.list[idx], trimmed, MAX_WORKSPACE_NAME_LEN - 1);
            config->workspaces.list[idx][MAX_WORKSPACE_NAME_LEN - 1] = '\0';
            token = strtok(NULL, ",");
            idx++;
        }
        free(copy);
    } else if (validating) {
        fprintf(stderr, "Line %d: Unknown key in [workspaces]: %s\n", line_num, key);
    }
}

bool read_config(FILE *file, struct fde_config *config) {
    if (!file || !config) {
        fprintf(stderr, "Invalid arguments to read_config\n");
        return false;
    }

    init_config_defaults(config);

    char line[512];
    char current_section[128] = {0};
    int line_num = 0;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        char *l = trim(line);

        if (*l == '\0' || *l == '#') continue;

        if (*l == '[') {
            char *end = strchr(l, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, l + 1, sizeof(current_section) - 1);
                current_section[sizeof(current_section) - 1] = '\0';
                trim(current_section);
                continue;
            } else {
                if (config->validating) {
                    fprintf(stderr, "Line %d: Invalid section header: %s\n", line_num, l);
                }
                continue;
            }
        }

        char *eq = strchr(l, '=');
        if (!eq) {
            if (config->validating) {
                fprintf(stderr, "Line %d: Invalid key=value line: %s\n", line_num, l);
            }
            continue;
        }

        *eq = '\0';
        char *key = trim(l);
        char *value = trim(eq + 1);

        bool section_found = false;
        for (size_t i = 0; i < sizeof(sections)/sizeof(sections[0]); i++) {
            if (strcmp(current_section, sections[i].section_name) == 0) {
                section_found = true;
                parse_key_value(key, value, sections[i].keys, sections[i].num_keys, config, config->validating, line_num);
                break;
            }
        }

        if (!section_found) {
            if (strcmp(current_section, "workspaces") == 0) {
                parse_workspaces_section(key, value, config, config->validating, line_num);
            } else if (config->validating && strlen(current_section) > 0) {
                fprintf(stderr, "Line %d: Unknown section: [%s]\n", line_num, current_section);
            }
        }
    }

    config->active = true;
    config->validating = false;

    return true;
}

void free_config(struct fde_config *config) {
    if (!config) return;
    free(config->plugins.dir);
    free(config->plugins.dir);
}

bool load_config(const char *path, struct fde_config *config) {
    if (!path || !config) {
        fprintf(stderr, "Invalid arguments to load_config\n");
        return false;
    }

    fde_log(FDE_INFO, "Loading config from %s", path);

    struct stat sb;
    if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode)) {
        fde_log(FDE_ERROR, "%s is a directory not a config file", path);
        return false;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        // TODO: Create config file from template
        // Auto-create default config.ini from template.
        // fde_log(FDE_INFO, "Config %s not found; creating default", path);
        // FILE *default_f = fopen(path, "w");
        // if (!default_f) {
        //     fde_log(FDE_ERROR, "Unable to create default config at %s", path);
        //     return false;
        // }

        // // Write the template (hardcoded string).
        // const char *template_str = 
        //     "# CONFIG TEMPLATE. CODE WILL AUTOMATICALLY PASTE IT IN XDG_CONFIG IF THERE IS NO config.ini INSIDE\n"
        //     "\n"
        //     "[plugins]\n"
        //     "dir=~/.config/fde/plugins/\n"
        //     "\n"
        //     "[hotreload]\n"
        //     "enabled=true\n"
        //     "scan_interval=5 # Fallback timer if no inotify. (Will be deleted after tests)\n";
        // fputs(template_str, default_f);
        // fflush(default_f);  // Ensure written.
        // fclose(default_f);

        // // Retry opening the created file.
        // f = fopen(path, "r");
        // if (!f) {
        //     fde_log(FDE_ERROR, "Unable to open newly created %s", path);
        //     return false;
        // }
        fde_log(FDE_ERROR, "Unable to open config file");
        return false;
    }

    bool config_load_success = read_config(f, config);
    fclose(f);

    if (!config_load_success) {
        fde_log(FDE_ERROR, "Error(s) loading config!");
    }

    return config->active || !config->validating || config_load_success;
}
