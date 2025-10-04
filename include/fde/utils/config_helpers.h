#include <ctype.h>
#include <fde/config.h>
#include <stdlib.h>
#include <string.h>

#define DEFINE_KEYS(name, keys) static config_key_desc_t name[] = { keys }

#define CONFIG_KEY(struct_type, key_name_str, type_enum, field_path) \
    { key_name_str, type_enum, __builtin_offsetof(struct_type, field_path) },

#define DEFINE_ALL_SECTIONS(...) \
    static config_section_desc_t sections[] = { __VA_ARGS__ }

#define SECTION_ENTRY(name, keys) \
    { name, keys, sizeof(keys)/sizeof(keys[0]) }

// Define keys array
DEFINE_KEYS(plugins_keys,
    CONFIG_KEY(struct fde_config, "dir", TYPE_STRING, plugins.dir)
);

DEFINE_KEYS(hotreload_keys,
    CONFIG_KEY(struct fde_config, "enabled", TYPE_BOOL, hr.enabled)
    CONFIG_KEY(struct fde_config, "scan_interval", TYPE_INT, hr.scan_interval)
);

// Define sections array
DEFINE_ALL_SECTIONS(
    SECTION_ENTRY("plugins", plugins_keys),
    SECTION_ENTRY("hotreload", hotreload_keys)
);

char *trim(char *str) {
    char *end;
    while (isspace((unsigned char)*str)) str++;
    if (*str == 0) return str;
    end = str + strlen(str) - 1;
    while (end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

static bool parse_value_string(const char *value, void *target) {
    char **str_target = (char **)target;
    free(*str_target);
    *str_target = strdup(value);
    return (*str_target != NULL);
}

static bool parse_value_bool(const char *value, void *target) {
    bool *bool_target = (bool *)target;
    if (strcmp(value, "true") == 0 || strcmp(value, "1") == 0 ||
        strcmp(value, "yes") == 0 || strcmp(value, "on") == 0) {
        *bool_target = true;
    } else if (strcmp(value, "false") == 0 || strcmp(value, "0") == 0 ||
               strcmp(value, "no") == 0 || strcmp(value, "off") == 0) {
        *bool_target = false;
    } else {
        return false;
    }
    return true;
}

static bool parse_value_int(const char *value, void *target) {
    int *int_target = (int *)target;
    char *endptr;
    long val = strtol(value, &endptr, 10);
    if (*endptr != '\0') return false;
    *int_target = (int)val;
    return true;
}

// Generic key-value parser
bool parse_key_value(const char *key, const char *value, config_key_desc_t *keys, size_t num_keys, struct fde_config *config, bool validating, int line_num) {
    for (size_t i = 0; i < num_keys; i++) {
        if (strcmp(key, keys[i].key_name) == 0) {
            void *target = (void *)((char *)config + keys[i].offset);
            bool success = false;
            switch (keys[i].type) {
                case TYPE_STRING:
                    success = parse_value_string(value, target);
                    break;
                case TYPE_BOOL:
                    success = parse_value_bool(value, target);
                    break;
                case TYPE_INT:
                    success = parse_value_int(value, target);
                    break;
                case TYPE_ARRAY:
                    // Not supported here
                    success = false;
                    break;
                default:
                    success = false;
            }
            if (!success && validating) {
                fprintf(stderr, "Line %d: Invalid value for key '%s': %s\n", line_num, key, value);
            }
            return success;
        }
    }
    if (validating) {
        fprintf(stderr, "Line %d: Unknown key: %s\n", line_num, key);
    }
    return false;
}