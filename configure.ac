AC_INIT([emux], [0.1], [sronsse@gmail.com])
AC_CONFIG_AUX_DIR([build])

AC_PROG_CC
AC_PROG_RANLIB

AC_CONFIG_SRCDIR([main/main.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile])

AM_INIT_AUTOMAKE([subdir-objects foreign -Wall -Werror])
AM_SILENT_RULES([yes])
AM_PROG_CC_C_O

PKG_PROG_PKG_CONFIG

# Execute menuconfig
kconfig-mconf Kconfig <& AS_ORIGINAL_STDIN_FD

# Set all CONFIG_xxx environment variables from the .config file if found
source $srcdir/.config

# Make sure at least one machine is selected
if test "$CONFIG_MACH" != "y"; then
	AC_MSG_ERROR([please select at least one machine.])
fi

# Helps defining CONFIG_xxx macros in config.h and automake conditionals
AC_DEFUN([AX_DECLARE_CONFIG], [
	AM_CONDITIONAL($1, test "$$1" = "y")
	if test "$$1" = "y"; then
		AC_DEFINE($1, 1, $1)
	fi
])

# Declare all our CONFIG_xxx variables
AX_DECLARE_CONFIG([CONFIG_MACH_NES])

AC_OUTPUT

