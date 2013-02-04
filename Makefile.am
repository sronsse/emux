bin_PROGRAMS = emux
emux_CFLAGS = -I$(srcdir)/include
emux_LDFLAGS = -Tlinker.lds
emux_SOURCES = main/machine.c main/main.c

# Machines
if CONFIG_MACH_NES
emux_SOURCES += mach/nes.c
endif

