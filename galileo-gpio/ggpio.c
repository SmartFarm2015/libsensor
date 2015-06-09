/*
 * Copyright (C) 2015, www.easyiot.com.cn
 *
 * The right to copy, distribute, modify, or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable license agreement.
 *
 */

#define _GNU_SOURCE // required by asprintf

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <string.h>
#include <errno.h>
#include <sys/timeb.h>
#include <json-c/json.h>
#include <time.h>

#include "ggpio.h"

static ggpio_t garray[] = {
	{
		.name = "a0",
		.inits = {
			{
				.n = 37,
				.v = 0,
				.d = "out",
			},
			{}
		},
		.map = -1,
	},
	{
		.name = "a1",
		.inits = {
			{
				.n = 36,
				.v = 0,
				.d = "out",
			},
			{}
		},
		.map = -1,
	},
	{
		.name = "a2",
		.inits = {
			{
				.n = 23,
				.v = 0,
				.d = "out",
			},
			{}
		},
		.map = -1,
	},
	{
		.name = "a3",
		.inits = {
			{
				.n = 22,
				.v = 0,
				.d = "out",
			},
			{}
		},
		.map = -1,
	},
	{
		.name = "a4",
		.inits = {
			{
				.n = 21,
				.v = 0,
				.d = "out",
			},
			{
				.n = 29,
				.v = 1,
				.d = "out",
			}
		},
		.map = -1,
	},
	{
		.name = "a5",
		.inits = {
			{
				.n = 20,
				.v = 0,
				.d = "out",
			},
			{
				.n = 29,
				.v = 1,
				.d = "out",
			}
		},
		.map = -1,
	},
	{
		.name = "d0",
		.inits = {
			{},
			{}
		},
		.map = 3,
	},
	{
		.name = "d1",
		.inits = {
			{
				.n = 31,
				.v = 0,
				.d = "out",
			},
			{}
		},
		.map = 14,
	},
	{
		.name = "d2",
		.inits = {
			{
				.n = 30,
				.v = 0,
				.d = "out",
			},
			{}
		},
		.map = 15,
	},
	{
		.name = "d3",
		.inits = {
			{},
			{}
		},
		.map = 28,
	},
	{
		.name = "d4",
		.inits = {
			{},
			{}
		},
		.map = 17,
	},
	{
		.name = "d5",
		.inits = {
			{},
			{}
		},
		.map = 24,
	},
	{
		.name = "d6",
		.inits = {
			{},
			{}
		},
		.map = 27,
	},
	{
		.name = "d7",
		.inits = {
			{},
			{}
		},
		.map = 26,
	},
	{
		.name = "d8",
		.inits = {
			{},
			{}
		},
		.map = 19,
	},
};

static inline ggpio_t * get_ggpio(const char *name)
{
	int i = 0;
	int n = sizeof(garray) / sizeof(ggpio_t);
	for (i = 0; i < n; i++) {
		if (strcmp(garray[i].name, name) == 0) {
			return &(garray[i]);
		}
	}

	return NULL;
}

static inline void dump_config(const char *name)
{
	ggpio_t *gp = get_ggpio(name);
	printf("+-------\n");
	printf("%s: %p\n", name, gp);
	printf("  name: %s\n", gp->name);
	printf("  inits[0]: %p\n", &(gp->inits[0]));
	printf("    n: %d\n", gp->inits[0].n);
	printf("    v: %d\n", gp->inits[0].v);
	printf("    d: %s\n", gp->inits[0].d);
	printf("  inits[1]: %p\n", &(gp->inits[1]));
	printf("    n: %d\n", gp->inits[1].n);
	printf("    v: %d\n", gp->inits[1].v);
	printf("    d: %s\n", gp->inits[1].d);
	printf("  map: %d\n", gp->map);
	printf("+-------\n");
}

static inline int __export_gpio(int n)
{
	char file[32] = {};
	char buf[8] = {};
	FILE *fp = NULL;

	/* export the gpio */
	memset(file, 0, 32);
	memset(buf, 0, 8);
	snprintf(file, 32, "/sys/class/gpio/export");
	snprintf(buf, 8, "%d", n);
	fp = fopen(file, "w");
	if (fp == NULL) {
		fprintf(stderr, "open file %s failed.\n", file);
		return -1;
	}
	fwrite(buf, sizeof(buf), 1, fp);
	fclose(fp);

	return 0;
}

static inline int __unexport_gpio(int n)
{
	char file[32] = {};
	char buf[8] = {};
	FILE *fp = NULL;

	/* export the gpio */
	memset(file, 0, 32);
	memset(buf, 0, 8);
	snprintf(file, 32, "/sys/class/gpio/unexport");
	snprintf(buf, 8, "%d", n);
	fp = fopen(file, "w");
	if (fp == NULL) {
		fprintf(stderr, "open file %s failed.\n", file);
		return -1;
	}
	fwrite(buf, sizeof(buf), 1, fp);
	fclose(fp);

	return 0;
}

static inline int __set_gpio_direction(int n, const char *dir)
{
	char file[48] = {};
	char buf[8] = {};
	FILE *fp = NULL;

	/* set direction */
	memset(file, 0, 48);
	memset(buf, 0, 8);
	snprintf(file, 48, "/sys/class/gpio/gpio%d/direction", n);
	snprintf(buf, 8, "%s", dir);
	fp = fopen(file, "w");
	if (fp == NULL) {
		fprintf(stderr, "open file %s failed.\n", file);
		return -1;
	}
	fwrite(buf, sizeof(buf), 1, fp);
	fclose(fp);

	return 0;
}

static inline int __read_from_gpio(int n)
{
	char file[32] = {};
	char buf[8] = {};
	FILE *fp = NULL;

	memset(file, 0, 32);
	memset(buf, 0, 8);
	snprintf(file, 32, "/sys/class/gpio/gpio%d/value", n);
	fp = fopen(file, "r");
	if (fp == NULL) {
		fprintf(stderr, "open file %s failed.\n", file);
		return -1;
	}
	fread(buf, sizeof(buf), 1, fp);
	fclose(fp);

	return atoi(buf);
}

static inline int __write_to_gpio(int n, unsigned int v)
{
	char file[32] = {};
	char buf[8] = {};
	FILE *fp = NULL;

	memset(file, 0, 32);
	memset(buf, 0, 8);
	snprintf(file, 32, "/sys/class/gpio/gpio%d/value", n);
	snprintf(buf, 8, "%d", v);
	fp = fopen(file, "w");
	if (fp == NULL) {
		fprintf(stderr, "open file %s failed.\n", file);
		return -1;
	}
	fwrite(buf, sizeof(buf), 1, fp);
	fclose(fp);

	return strlen(buf);
}

static inline int __do_one_gpio(int n, const char *d, int v)
{
	int ret = 0;

	/* export the gpio */
	ret = __export_gpio(n);
	if (ret < 0) {
		fprintf(stderr, "export gpio %d failed.\n", n);
		return ret;
	}

	ret = __set_gpio_direction(n, d);
	if (ret < 0) {
		fprintf(stderr, "set direction for gpio %d failed.\n", n);
		return ret;
	}

	if (strcmp(d, "out") == 0) {
		ret = __write_to_gpio(n, v);
		if (ret < 0) {
			fprintf(stderr, "set out value for gpio %d failed.\n", n);
			return ret;
		}
	} else if (strcmp(d, "in") == 0) {
		ret = __read_from_gpio(n);
	}

	ret = __unexport_gpio(n);
	return ret;
}

static int __init_one_function(ggpio_t *gp)
{
	int ret = 0;

	if (gp == NULL) {
		fprintf(stderr, "wrong parameter.\n");
		return -1;
	}

	if (gp->inits[0].n != 0 &&
	    gp->inits[0].d != NULL) {
		ret = __do_one_gpio(
			gp->inits[0].n,
			gp->inits[0].d,
			gp->inits[0].v);
	}

	if (ret < 0)
		return ret;

	if (gp->inits[1].n != 0 &&
	    gp->inits[1].d != NULL) {
		ret = __do_one_gpio(
			gp->inits[1].n,
			gp->inits[1].d,
			gp->inits[1].v);
	}

	return ret;
}

unsigned int galileo_analog_read(unsigned int an)
{
	char anstr[128] = {};
	int ret = 0;
	FILE *fp = NULL;
	char buf[8] = {};

	snprintf(anstr, 128, "a%d", an);
	ggpio_t *gp = get_ggpio(anstr);
	if (gp == NULL) {
		return 0;
	}

	ret = __init_one_function(gp);
	if (ret < 0) {
		fprintf(stderr, "Init function for analog %d failed.\n", an);
		return ret;
	}

	snprintf(anstr, 128, "/sys/bus/iio/devices/iio:device0/in_voltage%d_raw", an);
	fp = fopen(anstr, "r");
	if (fp == NULL) {
		fprintf(stderr, "Open file %s failed.\n", anstr);
		return -1;
	}
	fread(buf, sizeof(buf), 1, fp);
	fclose(fp);

	return atoi(buf);
}

int galileo_analog_write(unsigned int an, unsigned int v)
{
	char anstr[128] = {};
	int ret = 0;
	FILE *fp = NULL;
	char buf[8] = {};

	snprintf(anstr, 128, "a%d", an);
	ggpio_t *gp = get_ggpio(anstr);
	if (gp == NULL) {
		return 0;
	}

	ret = __init_one_function(gp);
	if (ret < 0) {
		return ret;
	}

	snprintf(anstr, 128, "/sys/bus/iio/devices/iio:device0/out_voltage%d_raw", an);
	snprintf(buf, 8, "%d", v);
	fp = fopen(anstr, "r");
	if (fp == NULL)
		return -1;
	fwrite(buf, sizeof(buf), 1, fp);
	fclose(fp);

	return 0;
}

unsigned int galileo_digital_read(unsigned int dn)
{
	char anstr[128] = {};
	int ret = 0;

	snprintf(anstr, 128, "d%d", dn);
	ggpio_t *gp = get_ggpio(anstr);
	if (gp == NULL) {
		return 0;
	}

	ret = __init_one_function(gp);
	if (ret < 0) {
		return ret;
	}

	return __do_one_gpio(gp->map, "in", 0);
}

int galileo_digital_write(unsigned int dn, unsigned int v)
{
	char anstr[128] = {};
	int ret = 0;

	snprintf(anstr, 128, "d%d", dn);
	ggpio_t *gp = get_ggpio(anstr);
	if (gp == NULL) {
		return 0;
	}

	ret = __init_one_function(gp);
	if (ret < 0) {
		return ret;
	}

	ret = __do_one_gpio(gp->map, "out", v);
	if (ret < 0) {
		fprintf(stderr, "%s: failed.", __func__);
	}

	return 0;
}

#if defined(GGPIO_TEST)
int main(void)
{
	dump_config("a0");
	int i = 0;
	do {
		int a0v = galileo_analog_read(0);
//		printf("a0v: %d\n", a0v);
		int a1v = galileo_analog_read(1);
//		printf("a1v: %d\n", a1v);
		int a2v = galileo_analog_read(2);
//		printf("a2v: %d\n", a2v);
		int a3v = galileo_analog_read(3);
//		printf("a3v: %d\n", a3v);
		int a4v = galileo_analog_read(4);
//		printf("a4v: %d\n", a4v);
		i++;
		if (a0v < 0 || a1v < 0 || a2v < 0 || a3v < 0 || a4v < 0) {
			printf("failed: %d\n", i);
			exit(-1);
		}
		if (i % 200 == 0) {
			printf("succeed: %d\n", i);
		}
	} while(1);

	return 0;
}
#endif
