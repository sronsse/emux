#include <stdio.h>
#include <string.h>
#include <cmdline.h>
#include <video.h>

extern struct video_frontend __video_frontends_begin, __video_frontends_end;

static struct video_frontend *frontend;

bool video_init(int width, int height)
{
	struct video_frontend *fe;
	char *name;

	if (frontend) {
		fprintf(stderr, "Video frontend already initialized!\n");
		return false;
	}

	/* Get selected frontend name */
	if (!cmdline_parse_string("video", &name)) {
		fprintf(stderr, "No video frontend selected!\n");
		return false;
	}

	/* Check if user specified/overrode some parameters first */
	cmdline_parse_int("width", &width);
	cmdline_parse_int("height", &height);

	/* Find frontend and initialize it */
	for (fe = &__video_frontends_begin; fe < &__video_frontends_end; fe++)
		if (!strcmp(name, fe->name)) {
			if (fe->init && fe->init(width, height)) {
				frontend = fe;
				return true;
			}
			return false;
		}

	/* Warn as video frontend was not found */
	fprintf(stderr, "Video frontend \"%s\" not recognized!\n", name);
	return false;
}

video_surface_t *video_create_surface(int width, int height)
{
	if (frontend->create_surface)
		return frontend->create_surface(width, height);
	return NULL;
}

void video_free_surface(video_surface_t *s)
{
	if (frontend->free_surface)
		frontend->free_surface(s);
}

void video_blit_surface(video_surface_t *s)
{
	if (frontend->blit_surface)
		frontend->blit_surface(s);
}

void video_update()
{
	if (frontend->update)
		frontend->update();
}

uint32_t video_map_rgb(video_surface_t *s, uint8_t r, uint8_t g, uint8_t b)
{
	if (frontend->map_rgb)
		return frontend->map_rgb(s, r, g, b);
	return 0;
}

uint32_t video_get_pixel(video_surface_t *s, int x, int y)
{
	if (frontend->get_pixel)
		return frontend->get_pixel(s, x, y);
	return 0;
}

void video_set_pixel(video_surface_t *s, int x, int y, uint32_t pixel)
{
	if (frontend->set_pixel)
		frontend->set_pixel(s, x, y, pixel);
}

void video_deinit()
{
	if (frontend->deinit)
		frontend->deinit();
	frontend = NULL;
}

