#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <SDL.h>
#define NO_SDL_GLEXT
#define GL_GLEXT_PROTOTYPES
#include <SDL_opengl.h>
#include <GL/glext.h>
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
	SDL_Surface *screen;
	uint8_t *pixels;
	int scale_factor;
	GLuint vbo;
	GLuint program;
	GLuint vertex_shader;
	GLuint fragment_shader;
	GLuint texture;
};

struct vertex {
	GLfloat position[NUM_POS_COORDS];
	GLfloat uv[NUM_TEX_COORDS];
};

static video_window_t *gl_init(int width, int height, int scale);
static void gl_deinit();
static void gl_update();
static struct color gl_get_pixel(int x, int y);
static void gl_set_pixel(int x, int y, struct color color);
static bool init_shaders();
static void init_buffers();
static void init_pixels();

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

static struct gl gl;

bool init_shaders()
{
	int status;

	/* Create program and associated shaders */
	gl.program = glCreateProgram();
	gl.vertex_shader = glCreateShader(GL_VERTEX_SHADER);
	gl.fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);

	/* Assign vertex and fragment shader sources */
	glShaderSource(gl.vertex_shader, 1, &vertex_source, NULL);
	glShaderSource(gl.fragment_shader, 1, &fragment_source, NULL);

	/* Compile shaders */
	glCompileShader(gl.vertex_shader);
	glCompileShader(gl.fragment_shader);

	/* Attach shaders and link program */
	glAttachShader(gl.program, gl.vertex_shader);
	glAttachShader(gl.program, gl.fragment_shader);
	glLinkProgram(gl.program);

	/* Verify link was successful */
	glGetProgramiv(gl.program, GL_LINK_STATUS, &status);
	return (status == GL_TRUE);
}

void init_buffers()
{
	int location;

	/* Generate and fill VBO */
	glGenBuffers(1, &gl.vbo);
	glBindBuffer(GL_ARRAY_BUFFER, gl.vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices,
		GL_STATIC_DRAW);

	/* Set position attribute */
	location = glGetAttribLocation(gl.program, "position");
	glEnableVertexAttribArray(location);
	glVertexAttribPointer(location,
		NUM_POS_COORDS,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct vertex),
		(GLvoid *)offsetof(struct vertex, position));

	/* Set texture coordinates attribute */
	location = glGetAttribLocation(gl.program, "uv");
	glEnableVertexAttribArray(location);
	glVertexAttribPointer(location,
		NUM_TEX_COORDS,
		GL_FLOAT,
		GL_FALSE,
		sizeof(struct vertex),
		(GLvoid *)offsetof(struct vertex, uv));
}

void init_pixels()
{
	int location;

	/* Initialize pixels */
	gl.pixels = malloc(gl.width * gl.height * 3 * sizeof(uint8_t));
	memset(gl.pixels, 0, gl.width * gl.height * 3 * sizeof(uint8_t));

	/* Generate and bind texture */
	glGenTextures(1, &gl.texture);
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, gl.texture);

	/* Set texture parameters */
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

	/* Allow texture 0 to be sampled from fragment shader */
	location = glGetUniformLocation(gl.program, "rubyTexture");
	glUniform1i(location, 0);

	/* Fill texture data */
	glTexImage2D(GL_TEXTURE_2D,
		0,
		GL_RGB,
		gl.width,
		gl.height,
		0,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		gl.pixels);
}

video_window_t *gl_init(int width, int height, int scale)
{
	Uint32 flags = SDL_OPENGL;

	/* Initialize video sub-system */
	if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
		fprintf(stderr, "Error initializing SDL video: %s\n",
			SDL_GetError());
		return NULL;
	}

	/* Set window position and title */
	SDL_putenv("SDL_VIDEO_CENTERED=center");
	SDL_WM_SetCaption("emux", NULL);

	/* Create main video surface */
	gl.screen = SDL_SetVideoMode(width * scale, height * scale, 0, flags);
	if (!gl.screen) {
		fprintf(stderr, "Error create video surface! %s\n",
			SDL_GetError());
		SDL_VideoQuit();
		return NULL;
	}

	/* Save parameters */
	gl.width = width;
	gl.height = height;
	gl.scale_factor = scale;

	/* Initialize shaders and return in case of failure */
	if (!init_shaders()) {
		fprintf(stderr, "Error initializing shaders!\n");
		return NULL;
	}

	/* Initialize buffers and pixels */
	init_buffers();
	init_pixels();

	return gl.screen;
}

void gl_update()
{
	/* Set viewport */
	glViewport(0,
		0,
		gl.width * gl.scale_factor,
		gl.height * gl.scale_factor);

	/* Clear screen */
	glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);

	/* Update texture data */
	glTexSubImage2D(GL_TEXTURE_2D,
		0,
		0,
		0,
		gl.width,
		gl.height,
		GL_RGB,
		GL_UNSIGNED_BYTE,
		gl.pixels);

	/* Set current program */
	glUseProgram(gl.program);

	/* Paint a quad with our texture on it */
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

	/* Set current program */
	glUseProgram(0);

	/* Flip screen */
	SDL_GL_SwapBuffers();
}

struct color gl_get_pixel(int x, int y)
{
	struct color color;

	/* Compute index of RGB triplet */
	int index = 3 * (x + y * gl.width);

	/* Fill color and return it */
	color.r = gl.pixels[index];
	color.g = gl.pixels[index + 1];
	color.b = gl.pixels[index + 2];
	return color;
}

void gl_set_pixel(int x, int y, struct color color)
{
	/* Compute index of RGB triplet */
	int index = 3 * (x + y * gl.width);

	/* Save pixel */
	gl.pixels[index] = color.r;
	gl.pixels[index + 1] = color.g;
	gl.pixels[index + 2] = color.b;
}

void gl_deinit()
{
	/* Free allocated components */
	free(gl.pixels);
	glDeleteBuffers(1, &gl.vbo);
	glDeleteTextures(1, &gl.texture);
	glDeleteShader(gl.vertex_shader);
	glDeleteShader(gl.fragment_shader);
	glDeleteProgram(gl.program);

	/* Quit SDL video sub-system */
	SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

VIDEO_START(opengl)
	.input = "sdl",
	.init = gl_init,
	.update = gl_update,
	.get_pixel = gl_get_pixel,
	.set_pixel = gl_set_pixel,
	.deinit = gl_deinit
VIDEO_END

