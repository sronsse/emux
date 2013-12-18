#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <audio.h>
#include <cmdline.h>
#include <config.h>
#include <machine.h>
#include <video.h>

#if (defined(CONFIG_AUDIO_SDL) || defined(CONFIG_VIDEO_SDL)) && \
	defined(__APPLE__)
#include <SDL.h>
#endif

static void print_usage(bool error);

void print_usage(bool error)
{
	FILE *stream = error ? stderr : stdout;
	struct list_link *link;
	struct machine *m;
	struct audio_frontend *af;
	struct video_frontend *vf;

	fprintf(stream, "Usage: emux [OPTION]...\n");
	fprintf(stream, "Emulates various machines (consoles, arcades).\n");

	/* Don't print full usage in case of error */
	if (error) {
		fprintf(stream, "Try `emux --help' for more information.\n");
		return;
	}

	/* Print options */
	fprintf(stream, "\n");
	fprintf(stream, "Emux options:\n");
	fprintf(stream, "  --machine=MACH    Selects machine to emulate\n");
	fprintf(stream, "  --audio=AUDIO     Selects audio frontend\n");
	fprintf(stream, "  --video=VIDEO     Selects video frontend\n");
	fprintf(stream, "  --scale=SCALE     Applies a screen scale ratio\n");
	fprintf(stream, "  --help            Display this help and exit\n");
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

int main(int argc, char *argv[])
{
	int i;

	/* Initialize random seed */
	srand(time(NULL));

	/* Print version and command line */
	fprintf(stdout, "Emux version %s\n", PACKAGE_VERSION);
	fprintf(stdout, "Command line:");
	for (i = 0; i < argc; i++)
		fprintf(stdout, " %s", argv[i]);
	fprintf(stdout, "\n");

	/* Initialize command line and parse it */
	cmdline_init(argc, argv);

	/* Check if user requires help */
	if (cmdline_parse_bool("help", NULL)) {
		print_usage(false);
		return 0;
	}

	/* Initialize machine */
	if (!machine_init()) {
		print_usage(true);
		return 1;
	}

	/* Run machine */
	machine_run();

	/* Deinitialize machine */
	machine_deinit();

	return 0;
}

