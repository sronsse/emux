bin_PROGRAMS = emux
emux_CFLAGS = -I$(srcdir)/include -Wall -Wextra -Werror \
	$(CACA_CFLAGS) \
	$(SDL_CFLAGS)
emux_LDFLAGS = -Tlinker.lds
emux_LDADD = -lrt $(CACA_LIBS) $(SDL_LIBS)
emux_SOURCES = main/clock.c \
	main/cmdline.c \
	main/controller.c \
	main/cpu.c \
	main/machine.c \
	main/main.c \
	main/memory.c \
	main/resource.c \
	main/video.c

# Machines
emux_SOURCES += mach/chip8.c
if CONFIG_MACH_NES
emux_SOURCES += mach/nes.c
endif

# Frontends
if CONFIG_VIDEO_CACA
emux_SOURCES += frontends/video/caca_video.c
endif
if CONFIG_VIDEO_SDL
emux_SOURCES += frontends/video/sdl_video.c
endif

# CPUs
emux_SOURCES += cpu/chip8_cpu.c
emux_SOURCES += cpu/rp2a03.c

# Controllers
emux_SOURCES += controllers/mapper/nes_mapper.c
emux_SOURCES += controllers/mapper/nrom.c
emux_SOURCES += controllers/video/ppu.c

