bin_PROGRAMS = emux
emux_CFLAGS = -I$(srcdir)/include -Wall -Wextra -Werror \
	$(ROXML_CFLAGS) \
	$(CACA_CFLAGS) \
	$(GL_CFLAGS) \
	$(GLU_CFLAGS) \
	$(SDL2_CFLAGS)
emux_LDADD = \
	$(CACA_LIBS) \
	$(CDIO_LIBS) \
	$(GL_LIBS) \
	$(GLU_LIBS) \
	$(SDL2_LIBS) \
	$(ROXML_LIBS) \
	-lm
emux_SOURCES = include/audio.h \
	include/bitops.h \
	include/cdrom.h \
	include/clock.h \
	include/cmdline.h \
	include/controller.h \
	include/cpu.h \
	include/env.h \
	include/event.h \
	include/file.h \
	include/input.h \
	include/list.h \
	include/log.h \
	include/machine.h \
	include/memory.h \
	include/port.h \
	include/resource.h \
	include/util.h \
	include/video.h \
	main/audio.c \
	main/cdrom.c \
	main/clock.c \
	main/cmdline.c \
	main/controller.c \
	main/cpu.c \
	main/env.c \
	main/event.c \
	main/file.c \
	main/input.c \
	main/log.c \
	main/machine.c \
	main/main.c \
	main/memory.c \
	main/port.c \
	main/resource.c \
	main/video.c
EXTRA_DIST = Kconfig \
	controllers/Kconfig \
	controllers/audio/Kconfig \
	controllers/cdrom/Kconfig \
	controllers/dma/Kconfig \
	controllers/input/Kconfig \
	controllers/mapper/Kconfig \
	controllers/media/Kconfig \
	controllers/serial/Kconfig \
	controllers/timer/Kconfig \
	controllers/video/Kconfig \
	cpu/Kconfig \
	frontends/Kconfig \
	frontends/audio/Kconfig \
	frontends/cdrom/Kconfig \
	frontends/input/Kconfig \
	frontends/video/Kconfig \
	include/libretro.h \
	libretro.c \
	link.T \
	Makefile.android_arm64-v8a \
	Makefile.android_armeabi \
	Makefile.android_armeabi-v7a \
	Makefile.android_mips \
	Makefile.android_mips64 \
	Makefile.android_x86 \
	Makefile.android_x86_64 \
	Makefile.common \
	Makefile.linux-portable_x86 \
	Makefile.linux-portable_x86_64 \
	Makefile.linux_x86 \
	Makefile.linux_x86_64 \
	Makefile.mingw_x86 \
	Makefile.mingw_x86_64 \
	Makefile.osx_x86 \
	Makefile.osx_x86_64 \
	Makefile.rules \
	Makefile.vita_arm \
	Makefile.wii_ppc \
	Makefile.windows_x86 \
	Makefile.windows_x86_64 \
	mach/Kconfig \
	mach/configs/all_defconfig \
	mach/configs/chip8_defconfig \
	mach/configs/gb_defconfig \
	mach/configs/nes_defconfig \
	mach/configs/sms_defconfig

# Machines
if CONFIG_MACH_CHIP8
emux_SOURCES += mach/chip8.c
endif
if CONFIG_MACH_GB
emux_SOURCES += mach/gb.c
endif
if CONFIG_MACH_NES
emux_SOURCES += mach/nes.c
endif
if CONFIG_MACH_SMS
emux_SOURCES += mach/sms.c
endif

# Frontends
if CONFIG_AUDIO_SDL
emux_SOURCES += frontends/audio/sdl_audio.c
endif
if CONFIG_CDROM_CDIO
emux_SOURCES += frontends/cdrom/cdio_cdrom.c
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
if CONFIG_VIDEO_OPENGL
emux_SOURCES += frontends/video/opengl_video.c
endif
if CONFIG_VIDEO_SDL
emux_SOURCES += frontends/video/sdl_video.c
endif

# CPUs
if CONFIG_CPU_CHIP8
emux_SOURCES += cpu/chip8_cpu.c
endif
if CONFIG_CPU_LR35902
emux_SOURCES += cpu/lr35902.c
endif
if CONFIG_CPU_RP2A03
emux_SOURCES += cpu/rp2a03.c
endif
if CONFIG_CPU_R3051
emux_SOURCES += cpu/r3051.c
endif
if CONFIG_CPU_Z80
emux_SOURCES += cpu/z80.c
endif

# Controllers
if CONFIG_CONTROLLER_AUDIO_APU
emux_SOURCES += controllers/audio/apu.c
endif
if CONFIG_CONTROLLER_AUDIO_PAPU
emux_SOURCES += controllers/audio/papu.c
endif
if CONFIG_CONTROLLER_AUDIO_SN76489
emux_SOURCES += controllers/audio/sn76489.c
endif
if CONFIG_CONTROLLER_AUDIO_SPU
emux_SOURCES += controllers/audio/spu.c
endif
if CONFIG_CONTROLLER_CDROM_PSX
emux_SOURCES += controllers/cdrom/psx_cdrom.c
endif
if CONFIG_CONTROLLER_DMA_NES
emux_SOURCES += controllers/dma/nes_sprite.c
endif
if CONFIG_CONTROLLER_DMA_PSX
emux_SOURCES += controllers/dma/psx_dma.c
endif
if CONFIG_CONTROLLER_INPUT_GB
emux_SOURCES += controllers/input/gb_joypad.c
endif
if CONFIG_CONTROLLER_INPUT_NES
emux_SOURCES += controllers/input/nes_controller.c
endif
if CONFIG_CONTROLLER_INPUT_SMS
emux_SOURCES += controllers/input/sms_controller.c
endif
if CONFIG_CONTROLLER_INPUT_PSX
emux_SOURCES += controllers/input/psx_controller.c
emux_SOURCES += controllers/input/psx_controller.h
emux_SOURCES += controllers/input/psx_dualshock.c
endif
if CONFIG_CONTROLLER_MAPPER_GB
emux_SOURCES += controllers/mapper/gb_mapper.c
emux_SOURCES += controllers/mapper/gb_mapper.h
endif
if CONFIG_CONTROLLER_MAPPER_MBC1
emux_SOURCES += controllers/mapper/mbc1.c
endif
if CONFIG_CONTROLLER_MAPPER_MMC1
emux_SOURCES += controllers/mapper/mmc1.c
endif
if CONFIG_CONTROLLER_MAPPER_MMC3
emux_SOURCES += controllers/mapper/mmc3.c
endif
if CONFIG_CONTROLLER_MAPPER_NES
emux_SOURCES += controllers/mapper/nes_mapper.c
emux_SOURCES += controllers/mapper/nes_mapper.h
endif
if CONFIG_CONTROLLER_MAPPER_NROM
emux_SOURCES += controllers/mapper/nrom.c
endif
if CONFIG_CONTROLLER_MAPPER_ROM
emux_SOURCES += controllers/mapper/rom.c
endif
if CONFIG_CONTROLLER_MAPPER_SEGA
emux_SOURCES += controllers/mapper/sega_mapper.c
endif
if CONFIG_CONTROLLER_MEDIA_MDEC
emux_SOURCES += controllers/media/mdec.c
endif
if CONFIG_CONTROLLER_MAPPER_SMS
emux_SOURCES += controllers/mapper/sms_mapper.c
emux_SOURCES += controllers/mapper/sms_mapper.h
endif
if CONFIG_CONTROLLER_SERIAL_GB
emux_SOURCES += controllers/serial/gb_serial.c
endif
if CONFIG_CONTROLLER_TIMER_GB
emux_SOURCES += controllers/timer/gb_timer.c
endif
if CONFIG_CONTROLLER_TIMER_PSX
emux_SOURCES += controllers/timer/psx_timer.c
endif
if CONFIG_CONTROLLER_VIDEO_GPU
emux_SOURCES += controllers/video/gpu.c
endif
if CONFIG_CONTROLLER_VIDEO_LCDC
emux_SOURCES += controllers/video/lcdc.c
endif
if CONFIG_CONTROLLER_VIDEO_PPU
emux_SOURCES += controllers/video/ppu.c
endif
if CONFIG_CONTROLLER_VIDEO_VDP
emux_SOURCES += controllers/video/vdp.c
endif

distclean-local:
	rm -f $(PWD)/.config $(PWD)/.config.old

