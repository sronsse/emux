#include <stdio.h>
#include <string.h>
#include <cmdline.h>
#include <input.h>
#include <video.h>
#ifdef __APPLE__
#include <mach-o/getsect.h>
#endif

#ifdef __APPLE__
void cx_video_frontends() __attribute__((__constructor__));
#endif

#if defined(_WIN32)
extern struct video_frontend _video_frontends_begin, _video_frontends_end;
static struct video_frontend *video_frontends_begin = &_video_frontends_begin;
static struct video_frontend *video_frontends_end = &_video_frontends_end;
#elif defined(__APPLE__)
struct video_frontend *video_frontends_begin;
struct video_frontend *video_frontends_end;
#else
extern struct video_frontend __video_frontends_begin, __video_frontends_end;
static struct video_frontend *video_frontends_begin = &__video_frontends_begin;
static struct video_frontend *video_frontends_end = &__video_frontends_end;
#endif

static struct video_frontend *frontend;

#ifdef __APPLE__
void cx_video_frontends()
{
#ifdef __LP64__
	const struct section_64 *sect;
#else
	const struct section *sect;
#endif
	sect = getsectbyname(VIDEO_SEGMENT_NAME, VIDEO_SECTION_NAME);
	video_frontends_begin = (struct video_frontend *)(sect->addr);
	video_frontends_end = (struct video_frontend *)(sect->addr +
		sect->size);
}
#endif

bool video_init(int width, int height)
{
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
	for (fe = video_frontends_begin; fe < video_frontends_end; fe++) {
		if (strcmp(name, fe->name))
			continue;

		if (fe->init) {
			/* Initialize video frontend */
			window = fe->init(width, height, scale);
			if (!window)
				return false;

			frontend = fe;

			/* Initialize input frontend */
			return input_init(name, window);
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

uint32_t video_map_rgb(uint8_t r, uint8_t g, uint8_t b)
{
	if (frontend->map_rgb)
		return frontend->map_rgb(r, g, b);
	return 0;
}

uint32_t video_get_pixel(int x, int y)
{
	if (frontend->get_pixel)
		return frontend->get_pixel(x, y);
	return 0;
}

void video_set_pixel(int x, int y, uint32_t pixel)
{
	if (frontend->set_pixel)
		frontend->set_pixel(x, y, pixel);
}

void video_deinit()
{
	if (frontend->deinit)
		frontend->deinit();
	input_deinit();
	frontend = NULL;
}

