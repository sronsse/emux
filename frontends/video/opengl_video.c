#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <SDL.h>
#define NO_SDL_GLEXT
#define GL_GLEXT_PROTOTYPES
#include <SDL_opengl.h>
#ifdef __APPLE__
#include <OpenGL/glext.h>
#else
#include <GL/glext.h>
#endif
#include <log.h>
#include <video.h>

/* Vertex parameters */
#define NUM_POS_COORDS	2
#define NUM_TEX_COORDS	2
#define MIN_POS		-1.0f
#define MAX_POS		1.0f
#define MIN_UV		0.0f
#define MAX_UV		1.0f

struct gl {
	int width;
	int height;
	int scale;
	uint8_t *pixels;
	GLuint vbo;
	GLuint program;
	GLuint vertex_shader;
	GLuint fragment_shader;
	GLuint texture;
	SDL_GLContext *context;
	SDL_Window *window;
};

struct vertex {
	GLfloat position[NUM_POS_COORDS];
	GLfloat uv[NUM_TEX_COORDS];
};

static window_t *gl_init(struct video_frontend *fe, struct video_specs *vs);
static void gl_deinit(struct video_frontend *fe);
static void gl_update(struct video_frontend *fe);
static struct color gl_get_p(struct video_frontend *fe, int x, int y);
static void gl_set_p(struct video_frontend *fe, int x, int y, struct color c);
static bool init_shaders(struct video_frontend *fe);
static void init_buffers(struct video_frontend *fe);
static void init_pixels(struct video_frontend *fe);

struct vertex vertices[] = {
	{ { MIN_POS, MAX_POS }, { MIN_UV, MIN_UV } },
	{ { MAX_POS, MAX_POS }, { MAX_UV, MIN_UV } },
	{ { MIN_POS, MIN_POS }, { MIN_UV, MAX_UV } },
	{ { MAX_POS, MIN_POS }, { MAX_UV, MAX_UV } }
};

static const char *vertex_source =
	"attribute vec4 position;"
	"attribute vec4 uv;"
	"void main()"
	"{"
	"	gl_Position =  position;"
	"	gl_TexCoord[0] = uv;"
	"}";

static const char *fragment_source =
	"uniform sampler2D texture;"
	"void main()"
	"{"
	"	gl_FragColor = texture2D(texture, gl_TexCoord[0].st);"
	"}";

bool init_shaders(struct video_frontend *fe)
{
	struct gl *gl = fe->priv_data;
	int status;

	/* Create program and associated shaders */
	gl->program = glCreateProgram();
	gl->vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	gl->fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	/* Assign vertex and fragment shader sources */
	glShaderSource(gl->vertex_shader, 1, &vertex_source, NULL);
	glShaderSource(gl->fragment_shader, 1, &fragment_source, NULL);

	/* Compile shaders */
	glCompileShader(gl->vertex_shader);
	glCompileShader(gl->fragment_shader);

	/* Attach shaders and link program */
	glAttachShader(gl->program, gl->vertex_shader);
	glAttachShader(gl->program, gl->fragment_shader);
	glLinkProgram(gl->program);

	/* Verify link was successful */
	glGetProgramiv(gl->program, GL_LINK_STATUS, &status);
	return (status == GL_TRUE);
}

void init_buffers(struct video_frontend *fe)
{
	struct gl *gl = fe->priv_data;
	int location;

	/* Generate and fill VBO */
	glGenBuffers(1, &gl->vbo);
	glBindBuffer(GL_ARRAY_BUFFER, gl->vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
		GL_STATIC_DRAW);

	/* Set position attribute */
	location = glGetAttribLocation(gl->program, "position");
	glEnableVertexAttribArray(location);
	glVertexAttribPointer(location,
		NUM_POS_COORDS,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct vertex),
		(GLvoid *)offsetof(struct vertex, position));

	/* Set texture coordinates attribute */
	location = glGetAttribLocation(gl->program, "uv");
	glEnableVertexAttribArray(location);
	glVertexAttribPointer(location,
		NUM_TEX_COORDS,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct vertex),
		(GLvoid *)offsetof(struct vertex, uv));
}

void init_pixels(struct video_frontend *fe)
{
	struct gl *gl = fe->priv_data;
	int location;

	/* Initialize pixels */
	gl->pixels = calloc(gl->width * gl->height * 3, sizeof(uint8_t));

	/* Generate and bind texture */
	glGenTextures(1, &gl->texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl->texture);

	/* Set texture parameters */
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	/* Allow texture 0 to be sampled from fragment shader */
	location = glGetUniformLocation(gl->program, "texture");
	glUniform1i(location, 0);

	/* Fill texture data */
	glTexImage2D(GL_TEXTURE_2D,
		0,
		GL_RGB,
		gl->width,
		gl->height,
		0,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		gl->pixels);
}

window_t *gl_init(struct video_frontend *fe, struct video_specs *vs)
{
	SDL_Window *window;
	SDL_GLContext *context;
	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	struct gl *gl;
	int w = vs->width;
	int h = vs->height;
	int s = vs->scale;

	/* Initialize video sub-system */
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		LOG_E("Error initializing SDL video: %s\n", SDL_GetError());
		return NULL;
	}

	/* Set window position and title */
	window = SDL_CreateWindow("emux",
		SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED,
		w * s,
		h * s,
		flags);
	if (!window) {
		LOG_E("Error creating window: %s\n", SDL_GetError());
		SDL_VideoQuit();
		return NULL;
	}

	/* Create an OpenGL context associated with the window */
	context = SDL_GL_CreateContext(window);
	if (!context) {
		LOG_E("Error creating GL Context: %s\n", SDL_GetError());
		SDL_VideoQuit();
		return NULL;
	}

	/* Create frontend private structure */
	gl = calloc(1, sizeof(struct gl));
	gl->width = w;
	gl->height = h;
	gl->scale = s;
	gl->context = context;
	gl->window = window;
	fe->priv_data = gl;

	/* Initialize shaders and return in case of failure */
	if (!init_shaders(fe)) {
		LOG_E("Error initializing shaders!\n");
		free(gl);
		SDL_VideoQuit();
		return NULL;
	}

	/* Initialize buffers and pixels */
	init_buffers(fe);
	init_pixels(fe);

	return window;
}

void gl_update(struct video_frontend *fe)
{
	struct gl *gl = fe->priv_data;

	/* Set viewport */
	glViewport(0, 0, gl->width * gl->scale, gl->height * gl->scale);

	/* Clear screen */
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	/* Update texture data */
	glTexSubImage2D(GL_TEXTURE_2D,
		0,
		0,
		0,
		gl->width,
		gl->height,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		gl->pixels);

	/* Set current program */
	glUseProgram(gl->program);

	/* Paint a quad with our texture on it */
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	/* Set current program */
	glUseProgram(0);

	/* Flip screen */
	SDL_GL_SwapWindow(gl->window);
}

struct color gl_get_p(struct video_frontend *fe, int x, int y)
{
	struct gl *gl = fe->priv_data;
	struct color c;

	/* Compute index of RGB triplet */
	int index = 3 * (x + y * gl->width);

	/* Fill color and return it */
	c.r = gl->pixels[index];
	c.g = gl->pixels[index + 1];
	c.b = gl->pixels[index + 2];
	return c;
}

void gl_set_p(struct video_frontend *fe, int x, int y, struct color c)
{
	struct gl *gl = fe->priv_data;

	/* Compute index of RGB triplet */
	int index = 3 * (x + y * gl->width);

	/* Save pixel */
	gl->pixels[index] = c.r;
	gl->pixels[index + 1] = c.g;
	gl->pixels[index + 2] = c.b;
}

void gl_deinit(struct video_frontend *fe)
{
	struct gl *gl = fe->priv_data;

	/* Free allocated components */
	free(gl->pixels);
	glDeleteBuffers(1, &gl->vbo);
	glDeleteTextures(1, &gl->texture);
	glDeleteShader(gl->vertex_shader);
	glDeleteShader(gl->fragment_shader);
	glDeleteProgram(gl->program);

	/* Free SDL resources */
	SDL_GL_DeleteContext(gl->context);
	SDL_DestroyWindow(gl->window);

	/* Quit SDL video sub-system */
	SDL_QuitSubSystem(SDL_INIT_VIDEO);

	/* Free private data */
	free(gl);
}

VIDEO_START(opengl)
	.input = "sdl",
	.init = gl_init,
	.update = gl_update,
	.get_p = gl_get_p,
	.set_p = gl_set_p,
	.deinit = gl_deinit
VIDEO_END

