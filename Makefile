#
# Makefile: This is the top-level Makefile
#
# Copyright (C) 2015, www.easyiot.com.cn
#

# install location
# use make PREFIX=/your/dir to override the default value
PREFIX	:= /usr

# Cross compiling and selecting different set of gcc/bin-utils
# ---------------------------------------------------------------------------
#
# When performing cross compilation for other architectures ARCH shall be set
# to the target architecture. (See arch/* for the possibilities).
# ARCH can be set during invocation of make:
# make ARCH=ia64
# Another way is to have ARCH set in the environment.
# The default ARCH is the host where make is executed.

# CROSS_COMPILE specify the prefix used for all executables used
# during compilation. Only gcc and related bin-utils executables
# are prefixed with $(CROSS_COMPILE).
# CROSS_COMPILE can be set on the command line
# make CROSS_COMPILE=ia64-linux-
# Alternatively CROSS_COMPILE can be set in the environment.
# Default value for CROSS_COMPILE is not to prefix executables
# Note: Some architectures assign CROSS_COMPILE in their arch/*/Makefile

#ARCH := arm

# use make all CROSS_COMPILE=i586-poky-linux- to override the default value
# 
CROSS_COMPILE :=
CROSS_SYSROOT :=
CROSS_CFLAGS :=

#
# --------------------------------------------------------------------------
# Enviroments
# --------------------------------------------------------------------------
#
TOPDIR := $(shell /bin/pwd)
ifndef TOPDIR
	TOPDIR := .
endif

export	TOPDIR

#
# Make variables (CC, etc...)
#
CC     := $(CROSS_COMPILE)gcc
CPP     := $(CROSS_COMPILE)gcc -E
AS      := $(CROSS_COMPILE)as
LD      := $(CROSS_COMPILE)ld
AR      := $(CROSS_COMPILE)ar
NM      := $(CROSS_COMPILE)nm
OBJCOPY := $(CROSS_COMPILE)objcopy
RANLIB  := $(CROSS_COMPILE)ranlib
STRIP   := $(CROSS_COMPILE)strip

LN = ln -s
RM = rm -f

export	CC CPP AS LD AR NM RANLIB OBJCOPY STRIP MAKEFILES LN RM

#
# The CFLAGS and LDFLAGS for compling and linking.
# NOTE: the sub-Makefile in the sub-directories must re-specify
# LDFLAGS to add their link scripts.
#
CFLAGS := -DPREFIX_DIR="$(PREFIX)"
CFLAGS += -Wall -Wno-deprecated-declarations
ifdef CROSS_CFLAGS
	CFLAGS += $(CROSS_CFLAGS)
endif
ifdef CROSS_SYSROOT
	CFLAGS += --sysroot=$(CROSS_SYSROOT)
endif

export CFLAGS
export CROSS_SYSROOT
export PREFIX

LDFLAGS       := -L$(TOPDIR) -L$(TOPDIR)/src
OBJCOPY_FLAGS := -R .note -R .comment -S


# --------------------------------------------------------
# make DEBUG=n will remove '-g'
# --------------------------------------------------------
ifeq ($(DEBUG), n)
	CFLAGS += -O2
else
	CFLAGS += -g
endif

export	CFLAGS LDFLAGS OBJCOPY_FLAGS

#
# ----------------------------------------------------------------------
# The default target of this Makefile, which is a convention of GNU make.
# ----------------------------------------------------------------------
#
all : samples
	@echo
	@echo "*** Build is completed successfully."
	@exit 0

.PHONY : samples
samples : virtsensor smartfarm

.PHONY: json-c
json-c:
	@echo "making json-c"
	$(MAKE) -C json-c

.PHONY : libsensor
libsensor : json-c
	$(MAKE) -C libsensor

.PHONY : libggpio
libggpio :
	$(MAKE) -C galileo-gpio

.PHONY : virtsensor
virtsensor : libsensor
	$(MAKE) -C virtsensor

.PHONY : smartfarm
smartfarm : libsensor libggpio
	$(MAKE) -C smartfarm

install :
	$(MAKE) -C libsensor install
	$(MAKE) -C virtsensor install

#
# ---------------------------------------------------------------------
# clean target: delete all intermediate object files.
# ---------------------------------------------------------------------
#
.PHONY: clean distclean
distclean clean:
	- find . -name "*.o" -exec rm -f {} \; > /dev/null 2>&1
	- find . -name "*.a" -exec rm -f {} \; > /dev/null 2>&1
	- find . -name ".depend" -exec rm -f {} \; > /dev/null 2>&1
	- find . -name "*~" -exec rm -f {} \; > /dev/null 2>&1
	$(RM) ./images/*.elf ./images/*.bin ./images/*.map ./images/*.srec
	$(MAKE) -C smartfarm clean
	$(MAKE) -C galileo-gpio clean
	$(MAKE) -C virtsensor clean
	$(MAKE) -C libsensor clean
	$(MAKE) -C json-c clean
	@echo
	@echo "*** All intermediate files have been removed !"
	@echo

