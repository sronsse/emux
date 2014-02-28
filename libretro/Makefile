
ifneq ($(EMSCRIPTEN),)
   platform = emscripten
endif

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
endif
endif

TARGET_NAME := emux

ifeq ($(platform), unix)
   EXTENSION := libretro.so
   fpic := -fPIC
   SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined
else ifeq ($(platform), osx)
   EXTENSION := libretro.dylib
   fpic := -fPIC
   SHARED := -dynamiclib
else ifeq ($(platform), ios)
   EXTENSION := libretro_ios.dylib
   fpic := -fPIC
   SHARED := -dynamiclib
   DEFINES := -DIOS
   CC = clang -arch armv7 -isysroot $(IOSSDK)
else ifeq ($(platform), qnx)
   EXTENSION := libretro_qnx.so
   fpic := -fPIC
   SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined
else ifeq ($(platform), emscripten)
   EXTENSION := libretro_emscripten.so
   fpic := -fPIC
   SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined
else
   CC = gcc
   EXTENSION := retro.dll
   SHARED := -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=link.T -Wl,--no-undefined
endif

override CFLAGS = -DLIBRETRO

ifeq ($(DEBUG), 1)
   override CFLAGS += -O0 -g
else
   override CFLAGS += -O3
endif

override CFLAGS += -Wall -I../include -I.. $(fpic)

ifeq ($(platform), qnx)
override CFLAGS += -Wc,-std=gnu99
else
override CFLAGS += -std=gnu99
endif

# Object files used in all implementations
COMMON_OBJS := ../frontends/input/retro_input.o \
   ../frontends/video/retro_video.o \
   ../main/audio.o \
   ../main/bitops.o \
   ../main/clock.o \
   ../main/cmdline.o \
   ../main/controller.o \
   ../main/cpu.o \
   ../main/env.o \
   ../main/file.o \
   ../main/input.o \
   ../main/log.o \
   ../main/list.o \
   ../main/machine.o \
   ../main/memory.o \
   ../main/resource.o \
   ../main/video.o

OBJECTS := $(COMMON_OBJS)

# Macro used to define a libretro target (name, valid extensions, objects)
define DEFINE_TARGET
$(TARGET_NAME)_$(1)_$(EXTENSION)_MACH := $(1)
$(TARGET_NAME)_$(1)_$(EXTENSION)_EXTS := $(2)
$(TARGET_NAME)_$(1)_$(EXTENSION)_OBJS := $(COMMON_OBJS) $(3)
TARGETS += $(TARGET_NAME)_$(1)_$(EXTENSION)
OBJECTS += $(3)
endef

# CHIP-8 target definition
$(eval $(call DEFINE_TARGET,chip8,ch8|bin|rom,\
   ../cpu/chip8_cpu.o \
   ../mach/chip8.o))

# GB target definition
$(eval $(call DEFINE_TARGET,gb,gb|bin|rom,\
   ../controllers/input/gb_joypad.o \
   ../controllers/mapper/gb_mapper.o \
   ../controllers/mapper/mbc1.o \
   ../controllers/mapper/rom.o \
   ../controllers/serial/gb_serial.o \
   ../controllers/timer/gb_timer.o \
   ../controllers/video/lcdc.o \
   ../cpu/lr35902.o \
   ../mach/gb.o))

# NES target definition
$(eval $(call DEFINE_TARGET,nes,nes|bin|rom,\
   ../controllers/dma/nes_sprite.o \
   ../controllers/input/nes_controller.o \
   ../controllers/mapper/nes_mapper.o \
   ../controllers/mapper/nrom.o \
   ../controllers/video/ppu.o \
   ../cpu/rp2a03.o \
   ../mach/nes.o))

all: $(TARGETS)

$(TARGETS): libretro.c $(OBJECTS)
	$(CC) $(CFLAGS) -DMACHINE=\"$($@_MACH)\" -DVALID_EXTS="\"$($@_EXTS)\"" -c -o libretro.o libretro.c
	$(CC) $(fpic) $(SHARED) $(INCLUDES) -o $@ libretro.o $($@_OBJS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f libretro.o $(OBJECTS) $(TARGETS)

.PHONY: clean

