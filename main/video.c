#include <stdio.h>
#include <string.h>
#include <cmdline.h>
#include <input.h>
#include <list.h>
#include <log.h>
#include <video.h>

/* Command-line parameters */
static char *video_fe_name;
PARAM(video_fe_name, string, "video", NULL, "Selects video frontend")
static int scale = 1;
PARAM(scale, int, "scale", NULL, "Applies a screen scale ratio")

struct list_link *video_frontends;
static struct video_frontend *frontend;

bool video_init(int width, int height)
{
	struct list_link *link = video_frontends;
	struct video_frontend *fe;
	int scale = 1;

	if (frontend) {
		LOG_E("Video frontend already initialized!\n");
		return false;
	}

	/* Validate video option */
	if (!video_fe_name) {
		LOG_E("No video frontend selected!\n");
		return false;
	}

	/* Validate scaling factor */
	if (scale <= 0) {
		LOG_E("Scaling factor should be positive!\n");
		return false;
	}

	/* Find video frontend */
	while ((fe = list_get_next(&link))) {
		if (strcmp(video_fe_name, fe->name))
			continue;

		if (fe->init) {
			/* Initialize video frontend */
			if (!fe->init(width, height, scale))
				return false;

			frontend = fe;

			/* Initialize input frontend */
			return input_init(fe->input);
		}
		return false;
	}

	/* Warn as video frontend was not found */
	LOG_E("Video frontend \"%s\" not recognized!\n", video_fe_name);
	return false;
}

video_window_t *video_get_window()
{
	if (frontend->get_window)
		return frontend->get_window();
	return NULL;
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

