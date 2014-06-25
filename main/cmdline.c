#include <getopt.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <audio.h>
#include <cmdline.h>
#include <config.h>
#include <list.h>
#include <machine.h>
#include <video.h>

#define QUOTE(name)	#name
#define STR(macro)	QUOTE(macro)
#define OPTION_DELIM	" "

struct cmdline {
	int argc;
	char **argv;
};

static void cmdline_build(int argc, char *argv[], int defc, char *defv[]);
static int param_sort_compare(const void *a, const void *b);
int param_bsearch_compare(const void *key, const void *elem);
static bool cmdline_parse_arg(char *long_name, bool has_arg, char **arg);
static bool cmdline_parse_bool(char *long_name, bool *arg);
static bool cmdline_parse_int(char *long_name, int *arg);
static bool cmdline_parse_string(char *long_name, char **string);

struct param **params;
static int num_params;
static struct cmdline cmdline;
static char def_cmdline[] = STR(CONFIG_CMDLINE);

int param_sort_compare(const void *a, const void *b)
{
	struct param *p1 = *(struct param **)a;
	struct param *p2 = *(struct param **)b;
	int result;

	/* Non-options come last */
	if (!p1->name)
		return 1;
	else if (!p2->name)
		return -1;

	/* Global options have a higher priority */
	if (!p1->module && p2->module)
		return -1;
	else if (p1->module && !p2->module)
		return 1;
	else if (!p1->module && !p2->module)
		return strcmp(p1->name, p2->name);

	/* Sort by module name */
	result = strcmp(p1->module, p2->module);
	if (result != 0)
		return result;

	/* Sort by parameter name */
	return strcmp(p1->name, p2->name);
}

int param_bsearch_compare(const void *key, const void *elem)
{
	char *module = (char *)key;
	struct param *p = *(struct param **)elem;

	/* Checked for global options first */
	if (!p->module)
		return 1;
	
	/* Check for module name next */
	return strcmp(module, p->module);
}

void cmdline_register_param(struct param *param)
{
	/* Grow params array, insert param, and sort array */
	params = realloc(params, ++num_params *	sizeof(struct param *));
	params[num_params - 1] = param;
	qsort(params, num_params, sizeof(struct param *), param_sort_compare);
}

void cmdline_unregister_param(struct param *param)
{
	int index;

	/* Find param to remove */
	for (index = 0; index < num_params; index++)
		if (params[index] == param)
			break;

	/* Return if param was not found */
	if (index == num_params)
		return;

	/* Shift remaining regions */
	while (index < num_params - 1) {
		params[index] = params[index + 1];
		index++;
	}

	/* Shrink params array */
	params = realloc(params, --num_params * sizeof(struct param *));
}

bool cmdline_set_param(char *name, char *module, char *value)
{
	struct param *p = NULL;
	int i;
	bool *bool_p;
	char **string_p;
	int *int_p;
	char *end;

	/* Find requested parameter */
	for (i = 0; i < num_params; i++) {
		p = params[i];

		/* Skip parameter if module presence does not match */
		if (!module != !p->module)
			continue;

		/* Skip parameter if name presence does not match */
		if (!name != !p->name)
			continue;

		/* Skip if module name does not match */
		if (module && strcmp(module, p->module))
			continue;

		/* Stop if parameter is found */
		if ((!name && !p->name) || !strcmp(name, p->name))
			break;
	}

	/* Leave if parameter was not found */
	if (i == num_params)
		return false;

	/* Set bool if needed */
	if (!strcmp(p->type, "bool")) {
		bool_p = p->address;
		*bool_p = !strcmp(value, "true");
		return true;
	}

	/* Set int if needed */
	if (!strcmp(p->type, "int")) {
		i = strtol(value, &end, 10);
		if (*end)
			return false;
		int_p = p->address;
		*int_p = i;
		return true;
	}

	/* Set string if needed */
	if (!strcmp(p->type, "string")) {
		string_p = p->address;
		*string_p = value;
		return true;
	}

	/* Parameter could not be converted */
	return false;
}

void cmdline_build(int argc, char *argv[], int defc, char *defv[])
{
#if defined(CONFIG_CMDLINE_EXTEND)
	/* Append passed command line to default one */
	cmdline.argc = defc + argc;
	cmdline.argv = malloc(cmdline.argc * sizeof(char *));
	cmdline.argv[0] = argv[0];
	memcpy(&cmdline.argv[1], defv, defc * sizeof(char *));
	memcpy(&cmdline.argv[defc + 1], &argv[1], (argc - 1) * sizeof(char *));
#elif defined(CONFIG_CMDLINE_FORCE)
	/* Force use of default command line */
	(void)argc;
	cmdline.argc = defc + 1;
	cmdline.argv = malloc(cmdline.argc * sizeof(char *));
	cmdline.argv[0] = argv[0];
	memcpy(&cmdline.argv[1], defv, defc * sizeof(char *));
#else
	/* Use passed command line if provided or default one otherwise */
	if (argc > 1) {
		cmdline.argc = argc;
		cmdline.argv = malloc(cmdline.argc * sizeof(char *));
		memcpy(cmdline.argv, argv, cmdline.argc * sizeof(char *));
	} else {
		cmdline.argc = defc + 1;
		cmdline.argv = malloc(cmdline.argc * sizeof(char *));
		cmdline.argv[0] = argv[0];
		memcpy(&cmdline.argv[1], defv, defc * sizeof(char *));
	}
#endif
}

void cmdline_init(int argc, char *argv[])
{
	char **defv = NULL;
	int defc = 0;
	struct param *p;
	char *s;
	int i;

	/* Build default command line as separate tokens */
	s = strtok(def_cmdline, OPTION_DELIM);
	while (s) {
		defv = realloc(defv, ++defc * sizeof(char *));
		defv[defc - 1] = s;
		s = strtok(NULL, OPTION_DELIM);
	}

	/* Build final command line based on project configuration */
	cmdline_build(argc, argv, defc, defv);

	/* Print version and command line first */
	fprintf(stdout, "Emux version %s\n", PACKAGE_VERSION);
	fprintf(stdout, "Command line:");
	for (i = 0; i < cmdline.argc; i++)
		fprintf(stdout, " %s", cmdline.argv[i]);
	fprintf(stdout, "\n");

	/* Parse and fill parameters */
	for (i = 0; i < num_params; i++) {
		p = params[i];
		if (!strcmp(p->type, "bool"))
			cmdline_parse_bool(p->name, p->address);
		else if (!strcmp(p->type, "int"))
			cmdline_parse_int(p->name, p->address);
		else if (!strcmp(p->type, "string"))
			cmdline_parse_string(p->name, p->address);
	}

	/* Free command lines */
	free(cmdline.argv);
	free(defv);
}

void cmdline_print_usage(bool error)
{
	FILE *stream = error ? stderr : stdout;
	struct list_link *link;
	struct param *p;
	struct machine *m;
	struct audio_frontend *af;
	struct video_frontend *vf;
	char str[20];
	int i;

	fprintf(stream, "Usage: emux [OPTION]... path\n");
	fprintf(stream, "Emulates various machines (consoles, arcades).\n");

	/* Don't print full usage in case of error */
	if (error) {
		fprintf(stream, "Try `emux --help' for more information.\n");
		return;
	}

	/* Print general options */
	fprintf(stream, "\n");
	fprintf(stream, "Emux options:\n");
	for (i = 0; i < num_params; i++) {
		/* Break when module-specific options are reached */
		p = params[i];
		if (p->module)
			break;

		/* Print argument name and type (not applicable to booleans) */
		if (!strcmp(p->type, "bool"))
			snprintf(str, 20, "%s", p->name);
		else
			snprintf(str, 20, "%s=%s", p->name, p->type);
		fprintf(stream, "  --%-20s", str);

		/* Print description */
		fprintf(stream, "%s\n", p->desc);
	}
	fprintf(stream, "\n");

	/* Print supported machines */
	link = machines;
	fprintf(stream, "Supported machines:\n");
	while ((m = list_get_next(&link)))
		fprintf(stream, "  %s (%s)\n", m->name, m->description);
	fprintf(stream, "\n");

	/* Print audio frontends */
	link = audio_frontends;
	fprintf(stream, "Audio frontends:\n");
	while ((af = list_get_next(&link)))
		fprintf(stream, "  %s\n", af->name);
	fprintf(stream, "\n");

	/* Print video frontends */
	link = video_frontends;
	fprintf(stream, "Video frontends:\n");
	while ((vf = list_get_next(&link)))
		fprintf(stream, "  %s\n", vf->name);
	fprintf(stream, "\n");

	/* Display project related info */
	fprintf(stream, "Report bugs to: sronsse@gmail.com\n");
	fprintf(stream, "Project page: <http://emux.googlecode.com>\n");
}

void cmdline_print_module_options(char *module)
{
	struct param *p;
	int i;

	/* Check if module has options */
	if (!bsearch(module,
		params,
		num_params,
		sizeof(struct param *),
		param_bsearch_compare))
		return;

	/* Print module options */
	fprintf(stderr, "\n");
	fprintf(stderr, "Valid %s options:\n", module);
	for (i = 0; i < num_params; i++) {
		p = params[i];
		if (p->module && !strcmp(p->module, module))
			fprintf(stderr, "  --%s (%s)\n", p->name, p->desc);
	}
	fprintf(stderr, "\n");
}

bool cmdline_parse_arg(char *long_name, bool has_arg, char **arg)
{
	int has_argument = has_arg ? required_argument : no_argument;
	struct option long_options[] = {
		{ long_name, has_argument, NULL, 'o' },
		{ 0, 0, 0, 0 }
	};
	int c;

	/* Reset option index and make sure errors don't get printed out */
	optind = 1;
	opterr = 0;

	/* Parse option until we either find requested one or none is left */
	do {
		c = getopt_long_only(cmdline.argc,
			cmdline.argv,
			"",
			long_options,
			NULL);

		/* Check if option is found */
		if (c == 'o') {
			if (has_arg)
				*arg = optarg;
			return true;
		}
	} while (c != -1);

	/* Check if a non-option is found if needed */
	if (!long_name && (optind < cmdline.argc)) {
		if (has_arg)
			*arg = cmdline.argv[optind];
		return true;
	}

	return false;
}

bool cmdline_parse_bool(char *long_name, bool *arg)
{
	bool b;

	b = cmdline_parse_arg(long_name, false, NULL);

	if (arg)
		*arg = b;
	return b;
}

bool cmdline_parse_int(char *long_name, int *arg)
{
	char *str;
	char *end;
	int i;

	if (!cmdline_parse_arg(long_name, true, &str))
		return false;

	i = strtol(str, &end, 10);
	if (*end)
		return false;

	if (arg)
		*arg = i;
	return true;
}

bool cmdline_parse_string(char *long_name, char **arg)
{
	char *str;

	if (!cmdline_parse_arg(long_name, true, &str))
		return false;

	if (arg)
		*arg = str;
	return true;
}

