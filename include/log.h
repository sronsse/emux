#ifndef _LOG_H
#define _LOG_H

#define LOG_D(fmt, ...) log_cb(LOG_DEBUG, fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) log_cb(LOG_INFO, fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) log_cb(LOG_WARNING, fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) log_cb(LOG_ERROR, fmt, ##__VA_ARGS__)

enum log_level {
	LOG_DEBUG,
	LOG_INFO,
	LOG_WARNING,
	LOG_ERROR,
	NUM_LOG_LEVELS
};

typedef void (*log_print_t)(enum log_level lvl, const char *fmt, ...);

extern log_print_t log_cb;

#endif

