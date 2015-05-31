/*
 * Copyright (C) 2015, www.easyiot.com.cn
 *
 * The right to copy, distribute, modify, or otherwise make use
 * of this software may be licensed only pursuant to the terms
 * of an applicable license agreement.
 *
 * Config file is in json format, includes the following nodes:
 *   host: Where the agent is on.
 *   port: Which port the agent is listening on
 *   datapoints: The datapoints managed by this datapoint application. The
 *   	initial datapoints node contains the properties of the datapoints.
 *   	After the datapoints are once registered to the agent, each datapoint
 *   	is allocated with one extra fields: id. So the next time this sensor app
 *   	starts, it knows that these datapoints are already registered
 *   	and will only let agent know that it is online.
 */

#define _GNU_SOURCE // required by asprintf

#include <stdio.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <windows.h>
#else
#include <unistd.h>
#include <getopt.h>
#endif
#include <string.h>
#include <sys/timeb.h>
#include "lib_sensor.h"

#ifdef _MSC_VER
int opterr;
int optind;
int optopt;
char *optarg;
int getopt(int argc, char **argv, char *opts);
#define NULL 0
#define EOF	(-1)
#define ERR(s, c)	if(opterr){\
	char errbuf[2];\
	errbuf[0] = c; errbuf[1] = '\n';\
	fputs(argv[0], stderr);\
	fputs(s, stderr);\
	fputc(c, stderr);}

int	opterr = 1;
int	optind = 1;
int	optopt;
char	*optarg;

int
getopt(argc, argv, opts)
int	argc;
char	**argv, *opts;
{
	static int sp = 1;
	register int c;
	register char *cp;

	if (sp == 1)
		if (optind >= argc ||
			argv[optind][0] != '-' || argv[optind][1] == '\0')
			return(EOF);
		else if (strcmp(argv[optind], "--") == NULL) {
			optind++;
			return(EOF);
		}
		optopt = c = argv[optind][sp];
		if (c == ':' || (cp = strchr(opts, c)) == NULL) {
			ERR(": illegal option -- ", c);
			if (argv[optind][++sp] == '\0') {
				optind++;
				sp = 1;
			}
			return('?');
		}
		if (*++cp == ':') {
			if (argv[optind][sp + 1] != '\0')
				optarg = &argv[optind++][sp + 1];
			else if (++optind >= argc) {
				ERR(": option requires an argument -- ", c);
				sp = 1;
				return('?');
			}
			else
				optarg = argv[optind++];
			sp = 1;
		}
		else {
			if (argv[optind][++sp] == '\0') {
				sp = 1;
				optind++;
			}
			optarg = NULL;
		}
		return(c);
}
#endif

const char *default_cfg = "virtsensor.json";

void usage()
{
	printf("Usage: [-h] [-f] [-c <configuration-file>]\n\n"
			"Options:\n"
			"  -c Configuration file.\n"
			"  -h Print this Help\n");
}

/*
 * Get datapoint data according to datapoint properties.
 * Here we fake the data using random numbers.
 */
void * get_datapoint_data(void *props)
{
	void *ret = NULL;
	const char *name = get_string_by_name(props, "name");

	if (strcmp(name, "temperature") == 0) {
		ret = malloc(sizeof(double));
		*(double *)ret = (150.0 * rand() / (RAND_MAX + 1.0) - 50);
	} else if (strcmp(name, "humidity") == 0) {
		ret = malloc(sizeof(double));
		*(double *)ret = (100.0 * rand() / (RAND_MAX + 1.0));
	} else if (strcmp(name, "image") == 0) {
		struct timeb t;
		ftime(&t);
		char *file = NULL;
		asprintf(&file, "image_%lld", 1000 * (long long)t.time + t.millitm);
		char buf[] = {'a','b','c'};
		FILE *fp = fopen(file, "w");
		fwrite (buf, sizeof(buf), 1, fp);
		fclose(fp);
		ret = (void*)file;
	}

	return ret;
}

int main(int argc, char *argv[])
{
	int ch = 0;
	const char *config_file = default_cfg;

	/* command line arguments processing */
	while ((ch=getopt(argc, argv, "hc:f")) != EOF) {
		switch (ch) {
			case 'h':
				usage();
				exit(0);
			case 'c':
				config_file = optarg;
				break;
			default:
				usage();
				exit(1);
		}
	}

	lib_sensor_start(config_file, get_datapoint_data, NULL, NULL);

	printf("sensor app is terminated!\n");
	return 0;
}
