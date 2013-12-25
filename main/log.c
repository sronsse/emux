#include <stdarg.h>
#include <stdio.h>
#include <cmdline.h>
#include <log.h>

static void log_fprint(enum log_level lvl, FILE *f, const char *fmt, va_list a);

/* Initialize default log level */
static enum log_level log_level = LOG_INFO;

/* Prefixes matching log level enumeration */
static char prefixes[] = {
	'D',
	'I',
	'W',
	'E'
};

void log_fprint(enum log_level lvl, FILE *f, const char *fmt, va_list a)
{
	/* Print to requested output */
	fprintf(f, "[%c] ", prefixes[(int)lvl]);
	vfprintf(f, fmt, a);
}

void log_init()
{
	int lvl;

	/* Get log level from command line or set default value */
	if (!cmdline_parse_int("log-level", &lvl))
		return;

	/* Validate argument */
	if ((lvl < 0) || (log_level >= NUM_LOG_LEVELS))
		return;

	/* Assign log level to user-specified level */
	log_level = lvl;
}

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

