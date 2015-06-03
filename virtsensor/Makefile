#
# Copyright (C) 2015, www.easyiot.com.cn
#

LDFLAGS :=
LDADD_FLAGS :=

ifndef CC
	CC := gcc
endif

ifndef CFLAGS
	CFLAGS := -Wall -Wno-deprecated-declarations -g
endif

CFLAGS += -I ../libsensor

LDFLAGS += -L../libsensor -lsensor -lm

BIN_PROGRAM := virtsensor

all: $(BIN_PROGRAM)

virtsensor : virtsensor.o
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

distclean clean:
	- find . -name "*.o" -exec rm -f {} \; > /dev/null 2>&1
	- rm -f $(BIN_PROGRAM)

install :
	cp $(BIN_PROGRAM) $(CROSS_SYSROOT)/bin

#
# ------------------------------------------------------------------
# Common rules...
# ------------------------------------------------------------------
#
.c.o :
	$(CC) -c $(CFLAGS) -o $@ $<

