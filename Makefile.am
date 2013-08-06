bin_PROGRAMS = emux
emux_CFLAGS = -I$(srcdir)/include -Wall -Wextra -Werror \
	$(ROXML_CFLAGS) \
	$(CACA_CFLAGS) \
	$(SDL_CFLAGS)
emux_LDFLAGS = $(ldscript_LDFLAGS)
emux_LDADD = $(CACA_LIBS) $(SDL_LIBS) $(ROXML_LIBS)
emux_SOURCES = include/audio.h \
	include/bitops.h \
	include/clock.h \
	include/cmdline.h \
	include/controller.h \
	include/cpu.h \
	include/input.h \
	include/list.h \
	include/machine.h \
	include/memory.h \
	include/resource.h \
	include/util.h \
	include/video.h \
	main/audio.c \
	main/bitops.c \
	main/clock.c \
	main/cmdline.c \
	main/controller.c \
	main/cpu.c \
	main/list.c \
	main/input.c \
	main/machine.c \
	main/main.c \
	main/memory.c \
	main/resource.c \
	main/video.c
EXTRA_DIST = Kconfig \
	linker.lds \
	controllers/Kconfig \
	controllers/dma/Kconfig \
	controllers/mapper/Kconfig \
	controllers/video/Kconfig \
	cpu/Kconfig \
	frontends/Kconfig \
	frontends/audio/Kconfig \
	frontends/input/Kconfig \
	frontends/video/Kconfig \
	mach/Kconfig

# Machines
if CONFIG_MACH_CHIP8
emux_SOURCES += mach/chip8.c
endif
if CONFIG_MACH_NES
emux_SOURCES += mach/nes.c
endif

# Frontends
if CONFIG_AUDIO_SDL
emux_SOURCES += frontends/audio/sdl_audio.c
endif
if CONFIG_INPUT_CACA
emux_SOURCES += frontends/input/caca_input.c
endif
if CONFIG_INPUT_SDL
emux_SOURCES += frontends/input/sdl_input.c
endif
if CONFIG_VIDEO_CACA
emux_SOURCES += frontends/video/caca_video.c
endif
if CONFIG_VIDEO_SDL
emux_SOURCES += frontends/video/sdl_video.c
endif

# CPUs
if CONFIG_CPU_CHIP8
emux_SOURCES += cpu/chip8_cpu.c
endif
if CONFIG_CPU_RP2A03
emux_SOURCES += cpu/rp2a03.c
endif

# Controllers
if CONFIG_CONTROLLER_DMA_NES
emux_SOURCES += controllers/dma/nes_sprite.c
endif
if CONFIG_CONTROLLER_MAPPER_NES
emux_SOURCES += controllers/mapper/nes_mapper.c
emux_SOURCES += include/controllers/mapper/nes_mapper.h
endif
if CONFIG_CONTROLLER_MAPPER_NROM
emux_SOURCES += controllers/mapper/nrom.c
endif
if CONFIG_CONTROLLER_VIDEO_PPU
emux_SOURCES += controllers/video/ppu.c
endif

clean-local:
	rm -f .config

