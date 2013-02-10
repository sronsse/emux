#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>
#include <cmdline.h>
#include <config.h>
#include <machine.h>

static void print_usage(bool error);

void print_usage(bool error)
{
	FILE *stream = error ? stderr : stdout;

	fprintf(stream, "Usage: emux [OPTION]...\n");
	fprintf(stream, "Emulates various machines (consoles, arcades).\n");

	/* Don't print full usage in case of error */
	if (error) {
		fprintf(stream, "Try `emux --help' for more information.\n");
		return;
	}

	fprintf(stream, "\n");
	fprintf(stream, "Emux options:\n");
	fprintf(stream, " -m, --machine=MACH  Selects machine to emulate\n");
	fprintf(stream, " -h, --help          Display this help and exit\n");
	fprintf(stream, "\n");
	fprintf(stream, "Valid machines:\n");
#ifdef CONFIG_MACH_NES
	fprintf(stream, " nes                 Nintendo Entertainment System\n");
#endif
	fprintf(stream, "\n");
	fprintf(stream, "Report bugs to: sronsse@gmail.com\n");
	fprintf(stream, "Project page: <https://github.com/sronsse/emux>\n");
}

int main(int argc, char *argv[])
{
	char *machine;
	int i;

	/* Print version and command line */
	fprintf(stdout, "Emux version %s\n", PACKAGE_VERSION);
	fprintf(stdout, "Command line:");
	for (i = 0; i < argc; i++)
		fprintf(stdout, " %s", argv[i]);
	fprintf(stdout, "\n");

	/* Initialize command line and parse it */
	cmdline_init(argc, argv);

	/* Check if user requires help */
	if (cmdline_parse_bool("help", 'h')) {
		print_usage(false);
		return 0;
	}

	/* Checks for machine selection */
	if (!cmdline_parse_string("machine", 'm', &machine)) {
		print_usage(true);
		return 1;
	}

	/* Initialize machine */
	if (!machine_init(machine))
		return 1;

	/* Deinitialize machine */
	machine_deinit();

	return 0;
}

