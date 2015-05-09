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

LIB_SENSOR := libsensor.so
LIB_SENSOR_H := lib_sensor.h

all: $(LIB_SENSOR)

libsensor.so: lib_sensor.o
	$(CC) $(CFLAGS) -o $@ $< ../json-c/libjson-c.a

distclean clean:
	- find . -name "*.o" -exec rm -f {} \; > /dev/null 2>&1
	- rm -f $(LIB_SENSOR)

install :
	cp $(LIB_SENSOR) $(CROSS_SYSROOT)$(PREFIX)/lib
	cp $(LIB_SENSOR_H) $(CROSS_SYSROOT)$(PREFIX)/include

#
# ------------------------------------------------------------------
# Common rules...
# ------------------------------------------------------------------
#
.c.o :
	$(CC) -c $(CFLAGS) -o $@ $<

