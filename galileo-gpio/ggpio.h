/*
 * Copyright (C) 2015, www.easyiot.com.cn
 *
 * The right to copy, distribute, modify, or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable license agreement.
 *
 * The simple library for ease the use of galileo's arduino
 * compatiable GPIO pins.
 */

#ifndef __LIB_GGPIO_H
#define __LIB_GGPIO_H

#define __stringify_1(x)    #x
#define __stringify(x)      __stringify_1(x)

struct gpio_init_one
{
	int n;
	int v;
	char *d;
};

typedef struct ggpio
{
	char *name;
	struct gpio_init_one inits[2];
	int map;
} ggpio_t;

unsigned int galileo_analog_read(unsigned int an);
int galileo_analog_write(unsigned int an, unsigned int v);
unsigned int galileo_digital_read(unsigned int dn);
int galileo_digital_write(unsigned int dn, unsigned int v);

#endif /* __LIB_GGPIO_H */
