#include <signal.h>
#include <stdarg.h>
#include <stdio.h>

#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <fde/utils/log.h>

static terminate_callback_t log_terminate = exit;

void _fde_abort(const char *format, ...) {
	va_list args;
	va_start(args, format);
	_fde_vlog(FDE_ERROR, format, args);
	va_end(args);
	log_terminate(EXIT_FAILURE);
}

bool _fde_assert(bool condition, const char *format, ...) {
	if (condition) {
		return true;
	}

	va_list args;
	va_start(args, format);
	_fde_vlog(FDE_ERROR, format, args);
	va_end(args);

#ifndef NDEBUG
	raise(SIGABRT);
#endif

	return false;
}

static bool colored = true;
static fde_log_importance_t log_importance = FDE_ERROR;
static struct timespec start_time = {-1, -1};

static const char *verbosity_colors[] = {
	[FDE_SILENT] = "",
	[FDE_ERROR ] = "\x1B[1;31m",
	[FDE_INFO  ] = "\x1B[1;34m",
	[FDE_DEBUG ] = "\x1B[1;90m",
};

static const char *verbosity_headers[] = {
	[FDE_SILENT] = "",
	[FDE_ERROR] = "[ERROR]",
	[FDE_INFO] = "[INFO]",
	[FDE_DEBUG] = "[DEBUG]",
};

static void timespec_sub(struct timespec *r, const struct timespec *a,
		const struct timespec *b) {
	const long NSEC_PER_SEC = 1000000000;
	r->tv_sec = a->tv_sec - b->tv_sec;
	r->tv_nsec = a->tv_nsec - b->tv_nsec;
	if (r->tv_nsec < 0) {
		r->tv_sec--;
		r->tv_nsec += NSEC_PER_SEC;
	}
}

static void init_start_time(void) {
	if (start_time.tv_sec >= 0) {
		return;
	}
	clock_gettime(CLOCK_MONOTONIC, &start_time);
}

static void fde_log_stderr(fde_log_importance_t verbosity, const char *fmt,
		va_list args) {
	init_start_time();

	if (verbosity > log_importance) {
		return;
	}

	struct timespec ts = {0};
	clock_gettime(CLOCK_MONOTONIC, &ts);
	timespec_sub(&ts, &ts, &start_time);

	fprintf(stderr, "%02d:%02d:%02d.%03ld ", (int)(ts.tv_sec / 60 / 60),
		(int)(ts.tv_sec / 60 % 60), (int)(ts.tv_sec % 60),
		ts.tv_nsec / 1000000);

	unsigned c = (verbosity < FDE_LOG_IMPORTANCE_LAST) ? verbosity :
		FDE_LOG_IMPORTANCE_LAST - 1;

	if (colored && isatty(STDERR_FILENO)) {
		fprintf(stderr, "%s", verbosity_colors[c]);
	} else {
		fprintf(stderr, "%s ", verbosity_headers[c]);
	}

	vfprintf(stderr, fmt, args);

	if (colored && isatty(STDERR_FILENO)) {
		fprintf(stderr, "\x1B[0m");
	}
	fprintf(stderr, "\n");
}

void fde_log_init(fde_log_importance_t verbosity, terminate_callback_t callback) {
	init_start_time();

	if (verbosity < FDE_LOG_IMPORTANCE_LAST) {
		log_importance = verbosity;
	}
	if (callback) {
		log_terminate = callback;
	}
}

void _fde_vlog(fde_log_importance_t verbosity, const char *fmt, va_list args) {
	fde_log_stderr(verbosity, fmt, args);
}

void _fde_log(fde_log_importance_t verbosity, const char *fmt, ...) {
	va_list args;
	va_start(args, fmt);
	fde_log_stderr(verbosity, fmt, args);
	va_end(args);
}

fde_log_importance_t convert_wlr_log_importance(enum wlr_log_importance importance) {
	switch (importance) {
	case WLR_ERROR:
		return FDE_ERROR;
	case WLR_INFO:
		return FDE_INFO;
	default:
		return FDE_DEBUG;
	}
}

void handle_wlr_log(enum wlr_log_importance importance, const char *fmt, va_list args) {
	static char fde_fmt[1024];
	snprintf(fde_fmt, sizeof(fde_fmt), "[wlr] %s", fmt);
	_fde_vlog(convert_wlr_log_importance(importance), fde_fmt, args);
}