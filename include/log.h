#ifndef _LOG_H
#define _LOG_H

#define LOG_D(...) log_cb(LOG_DEBUG, __VA_ARGS__)
#define LOG_I(...) log_cb(LOG_INFO, __VA_ARGS__)
#define LOG_W(...) log_cb(LOG_WARNING, __VA_ARGS__)
#define LOG_E(...) log_cb(LOG_ERROR, __VA_ARGS__)

enum log_level {
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR,
	NUM_LOG_LEVELS
};

typedef void (*log_print_t)(enum log_level lvl, const char *fmt, ...);

extern enum log_level log_level;
extern log_print_t log_cb;

#endif

