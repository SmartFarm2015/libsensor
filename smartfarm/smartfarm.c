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
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <sys/timeb.h>
#include "lib_sensor.h"

const char *default_cfg = "smartfarm.json";

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
		/* initially just use fake random value */
		ret = malloc(sizeof(double));
		*(double *)ret = (150.0 * rand() / (RAND_MAX + 1.0) - 50);
	} else if (strcmp(name, "image") == 0) {
		struct timeb t;
		ftime(&t);
		/* prepare a image file then return its name to libsensor */
		char *file = NULL;
		char *cmd = NULL;
		/* note, according to asprintf, the 'file' need be freed
		   later, which will be done by libsensor */
		asprintf(&file, "image_%lld%s", 1000 * (long long)t.time + t.millitm, ".jpg");
		asprintf(&cmd, "fswebcam --save %s 2>/dev/null", file);
		system(cmd);
		free(cmd);
		cmd = NULL;
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
