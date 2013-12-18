#include <stdio.h>
#include <string.h>
#include <cmdline.h>
#include <input.h>
#include <list.h>
#include <video.h>

struct list_link *video_frontends;
static struct video_frontend *frontend;

bool video_init(int width, int height)
{
	struct list_link *link = video_frontends;
	struct video_frontend *fe;
	char *name;
	int scale = 1;
	video_window_t *window;

	if (frontend) {
		fprintf(stderr, "Video frontend already initialized!\n");
		return false;
	}

	/* Get selected frontend name */
	if (!cmdline_parse_string("video", &name)) {
		fprintf(stderr, "No video frontend selected!\n");
		return false;
	}

	/* Check if user specified a video scaling factor */
	if (cmdline_parse_int("scale", &scale) && scale <= 0) {
		fprintf(stderr, "Scaling factor should be positive!\n");
		return false;
	}

	/* Find video frontend */
	while ((fe = list_get_next(&link))) {
		if (strcmp(name, fe->name))
			continue;

		if (fe->init) {
			/* Initialize video frontend */
			window = fe->init(width, height, scale);
			if (!window)
				return false;

			frontend = fe;

			/* Initialize input frontend */
			return input_init(fe->input, window);
		}
		return false;
	}

	/* Warn as video frontend was not found */
	fprintf(stderr, "Video frontend \"%s\" not recognized!\n", name);
	return false;
}

void video_update()
{
	if (frontend->update)
		frontend->update();

	/* Update input sub-system as well */
	input_update();
}

void video_lock()
{
	if (frontend->lock)
		frontend->lock();
}

void video_unlock()
{
	if (frontend->unlock)
		frontend->unlock();
}

struct color video_get_pixel(int x, int y)
{
	struct color default_color = { 0, 0, 0 };
	if (frontend->get_pixel)
		return frontend->get_pixel(x, y);
	return default_color;
}

void video_set_pixel(int x, int y, struct color color)
{
	if (frontend->set_pixel)
		frontend->set_pixel(x, y, color);
}

void video_deinit()
{
	if (frontend->deinit)
		frontend->deinit();
	input_deinit();
	frontend = NULL;
}

