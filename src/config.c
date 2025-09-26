#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>    // isspace для trim
#include <unistd.h>   // getuid для ~
#include <pwd.h>      // getpwuid
#include <sys/stat.h> // Для load_config

#include <fde/utils/log.h>
#include <fde/config.h>

// Helper: Trim leading/trailing spaces.
static char *trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

// Helper: Expand ~ to home dir (e.g., "~/.config" → "/home/user/.config").
static char *expand_tilde(const char *path) {
    if (!path || path[0] != '~') return strdup(path ? path : "");

    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw ? pw->pw_dir : "/home/user";  // Safe fallback.
    }
    char *full = malloc(strlen(home) + strlen(path));
    if (!full) return NULL;
    snprintf(full, strlen(home) + strlen(path) + 1, "%s%s", home, path + 1);  // +1 skip ~.
    return full;
}

// Helper: Safe malloc + log error.
static void *safe_malloc(size_t size, const char *what) {
    void *ptr = malloc(size);
    if (!ptr) {
        fde_log(FDE_ERROR, "Malloc failed for %s", what);
    }
    return ptr;
}

bool read_config(FILE *file, struct fde_config *config) {
    if (!file || !config) {
        fde_log(FDE_ERROR, "Invalid args to read_config");
        return false;
    }

    // Init state.
    config->validating = true;  // Strict mode (logs warnings).
    config->active = false;     // Will set to true on success.

    // Auto-allocate sub-structs if not present.
    if (!config->plugins) {
        config->plugins = safe_malloc(sizeof(struct plugins), "plugins struct");
        if (!config->plugins) return false;
        config->plugins->dir = NULL;
    }
    if (!config->hr) {
        config->hr = safe_malloc(sizeof(struct hotreload), "hotreload struct");
        if (!config->hr) return false;
        config->hr->enabled = true;      // Default.
        config->hr->scan_interval = 5;   // Default.
    }

    char line[512];
    char current_section[128] = {0};  // e.g., "plugins".
    bool has_plugins = false;
    bool has_hotreload = false;
    int line_num = 0;

    while (fgets(line, sizeof(line), file)) {
        line_num++;
        char *l = trim(line);

        // Skip empty or comment lines.
        if (*l == '\0' || *l == '#') continue;

        // Section: [section] → matches struct name.
        if (*l == '[') {
            char *end = strchr(l, ']');
            if (end) {
                *end = '\0';
                strncpy(current_section, l + 1, sizeof(current_section) - 1);  // Skip [.
                trim(current_section);
                continue;
            } else {
                if (config->validating) {
                    fde_log(FDE_INFO, "Line %d: Invalid section '%s'", line_num, l);
                }
                continue;
            }
        }

        // Key=value.
        char *eq = strchr(l, '=');
        if (!eq) {
            if (config->validating) {
                fde_log(FDE_INFO, "Line %d: Invalid key=value: %s", line_num, l);
            }
            continue;
        }

        *eq = '\0';  // Split key and value.
        char *key = trim(l);
        char *value = trim(eq + 1);

        // Parse based on section (case-sensitive match to struct names).
        if (strcmp(current_section, "plugins") == 0) {
            has_plugins = true;
            if (strcmp(key, "dir") == 0) {
                // Single string; expand tilde.
                free(config->plugins->dir);  // Free previous if any.
                config->plugins->dir = expand_tilde(value);
                if (!config->plugins->dir && config->validating) {
                    fde_log(FDE_ERROR, "Line %d: Failed to parse dir: %s", line_num, value);
                } else {
                    fde_log(FDE_DEBUG, "Parsed plugins.dir: %s", config->plugins->dir);
                }
            } else if (config->validating) {
                fde_log(FDE_INFO, "Line %d: Unknown key in [plugins]: %s", line_num, key);
            }
        } else if (strcmp(current_section, "hotreload") == 0) {
            has_hotreload = true;
            if (strcmp(key, "enabled") == 0) {
                config->hr->enabled = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
                                       strcmp(value, "yes") == 0 || strcmp(value, "on") == 0);
                fde_log(FDE_DEBUG, "Parsed hotreload.enabled: %s", config->hr->enabled ? "true" : "false");
            } else if (strcmp(key, "scan_interval") == 0) {
                config->hr->scan_interval = atoi(value);
                if (config->hr->scan_interval <= 0) {
                    config->hr->scan_interval = 5;  // Clamp to default.
                    if (config->validating) {
                        fde_log(FDE_INFO, "Line %d: Invalid scan_interval '%s'; using default 5", line_num, value);
                    }
                } else {
                    fde_log(FDE_DEBUG, "Parsed hotreload.scan_interval: %d", config->hr->scan_interval);
                }
            } else if (config->validating) {
                fde_log(FDE_INFO, "Line %d: Unknown key in [hotreload]: %s", line_num, key);
            }
        } else if (strlen(current_section) > 0 && config->validating) {
            fde_log(FDE_INFO, "Line %d: Unknown section: [%s]", line_num, current_section);
        }
    }

    // Validate: Log missing sections (optional; not fatal).
    if (config->validating) {
        if (!has_plugins) fde_log(FDE_INFO, "No [plugins] section; using defaults (dir=NULL)");
        if (!has_hotreload) fde_log(FDE_INFO, "No [hotreload] section; using defaults");
    }

    // Success: Set active if no fatal errors (e.g., malloc fails already returned false).
    config->active = true;
    config->validating = false;  // End strict mode.

    fde_log(FDE_INFO, "Config parsed successfully: active=%s", config->active ? "true" : "false");

    return true;  // Partial success (warnings OK).
}

void free_config(struct fde_config *config) {
    if (!config) return;

    // Free sub-structs.
    if (config->plugins) {
        free(config->plugins->dir);
        free(config->plugins);
        config->plugins = NULL;
    }
    if (config->hr) {
        free(config->hr);
        config->hr = NULL;
    }

    // Reset top-level.
    config->active = false;
    config->validating = false;

    fde_log(FDE_DEBUG, "Freed config resources");
}

bool load_config(const char *path, struct fde_config *config) {
    if (path == NULL) {
        fde_log(FDE_ERROR, "Unable to find a config file. Define it manually");
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
        // Auto-create default config.ini from template.
        fde_log(FDE_INFO, "Config %s not found; creating default", path);
        FILE *default_f = fopen(path, "w");
        if (!default_f) {
            fde_log(FDE_ERROR, "Unable to create default config at %s", path);
            return false;
        }

        // Write the template (hardcoded string).
        const char *template_str = 
            "# CONFIG TEMPLATE. CODE WILL AUTOMATICALLY PASTE IT IN XDG_CONFIG IF THERE IS NO config.ini INSIDE\n"
            "\n"
            "[plugins]\n"
            "dir=~/.config/fde/plugins/ # Auto (Code uses it)\n"
            "\n"
            "[hotreload]\n"
            "enabled=true\n"
            "scan_interval=5 # Fallback timer if no inotify. (Will be deleted after tests)\n";
        fputs(template_str, default_f);
        fflush(default_f);  // Ensure written.
        fclose(default_f);

        // Retry opening the created file.
        f = fopen(path, "r");
        if (!f) {
            fde_log(FDE_ERROR, "Unable to open newly created %s", path);
            return false;
        }
    }



    bool config_load_success = read_config(f, config);
    fclose(f);

    if (!config_load_success) {
        fde_log(FDE_ERROR, "Error(s) loading config!");
    }

    return config->active || !config->validating || config_load_success;
}

