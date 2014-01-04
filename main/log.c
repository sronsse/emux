#include <stdarg.h>
#include <stdio.h>
#include <cmdline.h>
#include <log.h>

static void log_print(enum log_level lvl, const char *fmt, ...);
static void log_fprint(enum log_level lvl, FILE *f, const char *fmt, va_list a);

log_print_t log_cb = log_print;

/* Command-line parameter */
static enum log_level log_level = LOG_INFO;
PARAM(log_level, int, "log-level", NULL, "Specifies log level (0 to 3)")

/* Prefixes matching log level enumeration */
static char prefixes[] = {
	'D',
	'I',
	'W',
	'E'
};

void log_print(enum log_level lvl, const char *fmt, ...)
{
	va_list args;

	/* Leave already if log level if insufficient */
	if (lvl < log_level)
		return;

	/* Print non-errors to stdout */
	if (lvl < LOG_ERROR) {
		va_start(args, fmt);
		log_fprint(lvl, stdout, fmt, args);
		va_end(args);
		return;
	}

	/* Print errors to stderr */
	if (lvl >= LOG_ERROR) {
		va_start(args, fmt);
		log_fprint(lvl, stdout, fmt, args);
		va_end(args);
	}
}

void log_fprint(enum log_level lvl, FILE *f, const char *fmt, va_list a)
{
	/* Print to requested output */
	fprintf(f, "[%c] ", prefixes[(int)lvl]);
	vfprintf(f, fmt, a);
}

