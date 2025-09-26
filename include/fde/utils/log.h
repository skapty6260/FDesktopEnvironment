#pragma once

#include <stdbool.h>
#include <stdarg.h>
// #include <string.h>
#include <wlr/util/log.h>
#include <errno.h>

typedef enum {
	FDE_SILENT = 0,
	FDE_ERROR = 1,
	FDE_INFO = 2,
	FDE_DEBUG = 3,
	FDE_LOG_IMPORTANCE_LAST,
} fde_log_importance_t;

#ifdef __GNUC__
#define ATTRIB_PRINTF(start, end) __attribute__((format(printf, start, end)))
#else
#define ATTRIB_PRINTF(start, end)
#endif

void error_handler(int sig);

typedef void (*terminate_callback_t)(int exit_code);

void fde_log_init(fde_log_importance_t verbosity, terminate_callback_t terminate);
fde_log_importance_t convert_wlr_log_importance(enum wlr_log_importance importance);

void _fde_log(fde_log_importance_t verbosity, const char *format, ...) ATTRIB_PRINTF(2, 3);
void _fde_vlog(fde_log_importance_t verbosity, const char *format, va_list args) ATTRIB_PRINTF(2, 0);
void _fde_abort(const char *filename, ...) ATTRIB_PRINTF(1, 2);
bool _fde_assert(bool condition, const char* format, ...) ATTRIB_PRINTF(2, 3);
void handle_wlr_log(enum wlr_log_importance importance, const char *fmt, va_list args);

#ifdef FDE_REL_SRC_DIR
// strip prefix from __FILE__, leaving the path relative to the project root
#define FDE_FILENAME ((const char *)__FILE__ + sizeof(FDE_REL_SRC_DIR) - 1)
#else
#define _FDE_FILENAME __FILE__
#endif

#define fde_log(verb, fmt, ...) \
	_fde_log(verb, "[%s:%d] " fmt, _FDE_FILENAME, __LINE__, ##__VA_ARGS__)

#define fde_vlog(verb, fmt, args) \
	_fde_vlog(verb, "[%s:%d] " fmt, _FDE_FILENAME, __LINE__, args)

#define fde_log_errno(verb, fmt, ...) \
	fde_log(verb, fmt ": %s", ##__VA_ARGS__, strerror(errno))

#define fde_abort(FMT, ...) \
	_fde_abort("[%s:%d] " FMT, _FDE_FILENAME, __LINE__, ##__VA_ARGS__)

#define fde_assert(COND, FMT, ...) _fde_assert(COND, "[%s:%d] %s:" FMT, _FDE_FILENAME, __LINE__, __PRETTY_FUNCTION__, ##__VA_ARGS__)