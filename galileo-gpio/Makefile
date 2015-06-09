#
# Copyright (C) 2015, www.easyiot.com.cn
#

# --------------------------------------------------------------------
# version info
# --------------------------------------------------------------------
MAJOR_VERSION := 1
MINOR_VERSION := 0
SUBLEVEL      := 0
VERSION := $(MAJOR_VERSION).$(MINOR_VERSION).$(SUBLEVEL)

LDFLAGS :=
LDADD_FLAGS :=

ifndef CC
	CC := gcc
endif

ifndef CFLAGS
	CFLAGS := -Wall -Wno-deprecated-declarations -g
endif

CFLAGS += -I ../ -fPIC -rdynamic -shared -DVERSION="$(VERSION)"

LIB_GGPIO := libggpio.so
LIB_GGPIO_H := ggpio.h

all: $(LIB_GGPIO) test

test: ggpio.c
	$(CC) -DGGPIO_TEST -o $@ $< ../json-c/libjson-c.a

$(LIB_GGPIO): ggpio.o
	$(CC) $(CFLAGS) -o $@ $< ../json-c/libjson-c.a

distclean clean:
	- find . -name "*.o" -exec rm -f {} \; > /dev/null 2>&1
	- rm -f $(LIB_GGPIO)

install :
	cp $(LIB_GGPIO) $(CROSS_SYSROOT)/lib
	cp $(LIB_GGPIO_H) $(CROSS_SYSROOT)/include

#
# ------------------------------------------------------------------
# Common rules...
# ------------------------------------------------------------------
#
.c.o :
	$(CC) -c $(CFLAGS) -o $@ $<

