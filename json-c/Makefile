#
# Copyright (C) 2015, www.easyiot.com.cn
#

LDFLAGS :=
LDADD_FLAGS :=

ifndef CC
	CC := gcc
	AR := ar
endif

ifndef CFLAGS
	CFLAGS := -Wall -Wno-deprecated-declarations -g
endif

CFLAGS += -fPIC

JSON_C_OBJS = \
	arraylist.o \
	debug.o \
	json_c_version.o \
	json_object.o \
	json_object_iterator.o \
	json_tokener.o \
	json_util.o \
	linkhash.o \
	printbuf.o \
	random_seed.o


LIB_JSON_C := libjson-c.a

all: $(LIB_JSON_C)

libjson-c.a: $(JSON_C_OBJS)
	$(AR) -r $(LIB_JSON_C) $(JSON_C_OBJS)

distclean clean:
	- find . -name "*.o" -exec rm -f {} \; > /dev/null 2>&1
	- rm -f $(LIB_JSON_C)

#
# ------------------------------------------------------------------
# Common rules...
# ------------------------------------------------------------------
#
.c.o :
	$(CC) -c $(CFLAGS) -o $@ $<

