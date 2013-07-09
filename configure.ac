AC_INIT([emux], [0.1], [sronsse@gmail.com])
AC_CONFIG_AUX_DIR([build])

# Retrieve target os
AC_CANONICAL_TARGET

AC_PROG_CC
AC_PROG_RANLIB

AC_CONFIG_SRCDIR([main/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror])
AM_SILENT_RULES([yes])
AM_PROG_CC_C_O

PKG_PROG_PKG_CONFIG

# Export source directory to the environment
export EMUX_SRC_DIR=$srcdir

# Declare linker script if arch requires it
AS_CASE(["$target_os"], [*darwin*], [ldscript_LDFLAGS=""],
	[ldscript_LDFLAGS="-T${EMUX_SRC_DIR}/linker.lds"])
AC_SUBST([ldscript_LDFLAGS], [${ldscript_LDFLAGS}])

# Execute menuconfig
kconfig-mconf $EMUX_SRC_DIR/Kconfig <& AS_ORIGINAL_STDIN_FD

# Set all CONFIG_xxx variables from .config file if found, otherwise exit
test -f $PWD/.config && source $PWD/.config || exit

# Make sure at least one audio frontend is selected
if test "$CONFIG_AUDIO" != "y"; then
	AC_MSG_ERROR([please select at least one audio frontend.])
fi

# Make sure at least one input frontend is selected
if test "$CONFIG_INPUT" != "y"; then
	AC_MSG_ERROR([please select at least one input frontend.])
fi

# Make sure at least one video frontend is selected
if test "$CONFIG_VIDEO" != "y"; then
	AC_MSG_ERROR([please select at least one video frontend.])
fi

# Make sure at least one machine is selected
if test "$CONFIG_MACH" != "y"; then
	AC_MSG_ERROR([please select at least one machine.])
fi

# Check if libroxml is present
PKG_CHECK_MODULES([ROXML], [libroxml])

# Add libcaca if needed
if test "$CONFIG_INPUT_CACA" == "y" || test "$CONFIG_VIDEO_CACA" == "y"; then
PKG_CHECK_MODULES([CACA], [caca])
fi

# Add SDL if needed
if test "$CONFIG_AUDIO_SDL" == "y" ||
	test "$CONFIG_INPUT_SDL" == "y" ||
	test "$CONFIG_VIDEO_SDL" == "y"; then
SDL_VERSION=1.2.0
AM_PATH_SDL($SDL_VERSION, :, AC_MSG_ERROR([*** SDL $SDL_VERSION not found!]))
fi

# Helps defining CONFIG_xxx macros in config.h and automake conditionals
AC_DEFUN([AX_DECLARE_CONFIG], [
	AM_CONDITIONAL($1, test "$$1" = "y")
	if test "$$1" = "y"; then
		AC_DEFINE($1, 1, $1)
	fi
])

# Declare all our CONFIG_xxx variables
AX_DECLARE_CONFIG([CONFIG_AUDIO_SDL])
AX_DECLARE_CONFIG([CONFIG_INPUT_CACA])
AX_DECLARE_CONFIG([CONFIG_INPUT_SDL])
AX_DECLARE_CONFIG([CONFIG_VIDEO_CACA])
AX_DECLARE_CONFIG([CONFIG_VIDEO_SDL])
AX_DECLARE_CONFIG([CONFIG_CPU_CHIP8])
AX_DECLARE_CONFIG([CONFIG_CPU_RP2A03])
AX_DECLARE_CONFIG([CONFIG_CONTROLLER_DMA_NES])
AX_DECLARE_CONFIG([CONFIG_CONTROLLER_MAPPER_NES])
AX_DECLARE_CONFIG([CONFIG_CONTROLLER_MAPPER_NROM])
AX_DECLARE_CONFIG([CONFIG_CONTROLLER_VIDEO_PPU])
AX_DECLARE_CONFIG([CONFIG_MACH_CHIP8])
AX_DECLARE_CONFIG([CONFIG_MACH_NES])

AC_OUTPUT

