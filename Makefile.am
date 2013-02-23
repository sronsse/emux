bin_PROGRAMS = emux
emux_CFLAGS = -I$(srcdir)/include
emux_LDFLAGS = -Tlinker.lds
emux_SOURCES = main/cmdline.c \
	main/controller.c \
	main/cpu.c \
	main/machine.c \
	main/main.c \
	main/memory.c

# Machines
if CONFIG_MACH_NES
emux_SOURCES += mach/nes.c
endif

# CPUs
emux_SOURCES += cpu/rp2a03.c

# Controllers
emux_SOURCES += controllers/video/ppu.c

