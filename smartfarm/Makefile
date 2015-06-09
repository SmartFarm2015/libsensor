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

CFLAGS += -I ../libsensor -I../galileo-gpio

LDFLAGS += -L../libsensor -lsensor -L../galileo-gpio -lggpio -lm -lpthread -ldl

BIN_PROGRAM := smartfarm

all: $(BIN_PROGRAM)

smartfarm : smartfarm.o
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

