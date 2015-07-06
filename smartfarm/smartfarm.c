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
#include <errno.h>
#include <sys/timeb.h>
#include <math.h>
#include "lib_sensor.h"
#include "ggpio.h"
#include <modbus/modbus.h>

#include <strings.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>

#define BAUDRATE B9600

#define _POSIX_SOURCE 1 /* POSIX compliant source */

const char *default_cfg = "smartfarm.json";

void usage()
{
	printf("Usage: [-h] [-f] [-c <configuration-file>]\n\n"
			"Options:\n"
			"  -c Configuration file.\n"
			"  -h Print this Help\n");
}

static void clean_mb_ctx(modbus_t *ctx)
{
    modbus_close(ctx);
    modbus_free(ctx);
}

static modbus_t * init_mb_serial(const char * serial_dev, uint8_t slave_addr)
{
    modbus_t *ctx;

    ctx = modbus_new_rtu(serial_dev, 9600, 'N', 8, 1);

    if (ctx == NULL) {
        fprintf(stderr, "Unable to allocate libmodbus context\n");
        return NULL;
    }
    modbus_set_debug(ctx, FALSE);
    modbus_set_error_recovery(ctx,
                              MODBUS_ERROR_RECOVERY_LINK |
                              MODBUS_ERROR_RECOVERY_PROTOCOL);

    modbus_set_slave(ctx, 0xFF);

    if (modbus_connect(ctx) == -1) {
        fprintf(stderr, "Connection failed: %s\n", modbus_strerror(errno));
        modbus_free(ctx);
        return NULL;
    }

    modbus_set_slave(ctx, slave_addr);

    return ctx;
}

static int get_mb_reg(modbus_t *ctx, int reg_addr, int data_len, void *data)
{
    int rc, ret = -1;

    for (;;) {
        rc = modbus_read_registers(ctx, reg_addr, data_len, data);
        if (rc != data_len) {
            break;
        } else {
            ret = 0;
            /**
             * If do not add the below usleep and break, it also works, but
             * would take more time hence slower on communication.
             */
            usleep(3000);
            break;
        }
    }

    return ret;
}

int get_mbsensor_data(const char *mb_device, int dev_addr, int reg_addr)
{
    int mb_addr = dev_addr; // 485 slave address, 3
    int mb_reg_addr = reg_addr; // reg address 0:humi, 1:temp
    int mb_reg_data_len = 1; // register data length, int16=2Byte

    double ret = 0;
    int rc = -1;

    uint16_t *_data = malloc(sizeof(uint16_t) * mb_reg_data_len);
    if (_data == NULL) {
	printf("mbsensor error: alloc memory error.");
	ret = -1;
	goto out_clean;
    }

    *_data = 0;


    modbus_t *ctx = NULL;
    int retry_read = 0;

mb_read_once:
    /*
     * 初始化RS485和ModBus通信
     */
    ctx = init_mb_serial(mb_device, mb_addr);
    if (ctx == NULL) {
	printf("mbsensor errot: modbus context error.");
	ret = -1;
	goto out_clean;
    }

    /*
     * 根据数据点的属性定义，从给定的modbus 从设备(modbus slave device)的特定寄存器中读取寄存器值,
     * 并对读出的值做简单数据处理。
     */
    rc = get_mb_reg(ctx, mb_reg_addr, mb_reg_data_len, _data);
    printf("mbsensor read: dev_address=%d, reg_address=%2X, reg_value=%04X\n", mb_addr, mb_reg_addr, *_data);

    if (rc == 0) {
	clean_mb_ctx(ctx);
	ret = (int)(*_data);
    } else {
	/* get data failed. */
	clean_mb_ctx(ctx);
	printf("mbsensor error: read error. retry %d\n", retry_read);
	if (retry_read++ < 6) {
	    goto mb_read_once;
	}
    }

out_clean:
    if (_data != NULL)
	free(_data);

    return ret;
}

// serial read for PM2.5 and PM10
static struct termios oldtio;
static int init_serial_port(const char *port)
{
	int fd;
	struct termios newtio;

	fd = open(port, O_RDWR | O_NOCTTY );
	if (fd < 0) {
		perror(port);
		return -1;
	}

	/* save current port settings */
	tcgetattr(fd, &oldtio);
	bzero(&newtio, sizeof(newtio));

	newtio.c_cflag = BAUDRATE | CRTSCTS | CS8 | CLOCAL | CREAD;
	newtio.c_iflag = IGNPAR;
	newtio.c_oflag = 0;

	/* set input mode (non-canonical, no echo,...) */
	newtio.c_lflag = 0;
	/* inter-character timer unused */
	newtio.c_cc[VTIME]    = 0;
	/* blocking read until 5 chars received */
	newtio.c_cc[VMIN]     = 1;

	tcflush(fd, TCIFLUSH);

	tcsetattr(fd,TCSANOW,&newtio);

	return fd;
}

static int clean_serial_port(int fd)
{
	tcsetattr(fd, TCSANOW, &oldtio);
	close(fd);

	return 0;
}

static void dump_str(unsigned char *cmd, int len)
{
	int i = 0;
	printf("buf_len: %d\n", len);

	while(i < len) {
		printf("%02x ", cmd[i++]);
	}
	printf("\n");
}

static int do_read_from_serial(unsigned char *buf, int fd)
{
	/* file descriptor set */
	fd_set readfds;
	int fdmax = fd;
	int ret = 0;
	int buf_len = 0;
	int res;

	FD_ZERO(&readfds);
	FD_SET(fd, &readfds);

	/* 5s timeout */
	struct timeval timeout = {5, 0};
	ret = select(fdmax + 1, &readfds, NULL, NULL, &timeout);
	if (ret != 0) {
		if (FD_ISSET(fd, &readfds)) {
			buf_len = 0;

			do {
				ret = select(fdmax + 1, &readfds, NULL, NULL, &timeout);
				if (ret != 1) {
					/* timeout */
					break;
				}

				res = read(fd, buf + buf_len, 5);
				/* returns after 5 chars have been input */
				buf_len += res;
			} while (buf_len < 50);
		}
	}

	return buf_len;
}

// get PM2.5 and PM10
// data packet length: 24 bytes
// 42 4D 00 14 00 1F 00 28 00 30 00 1A 00 24 00 2F 00 00 00 00 00 54 01 DB 
// data[0] = 0x42
// PM2.5 = data[6]<<8 + data[7];
// PM10  = data[8]<<8 + data[9];
int get_airpmindex(const char *dev_name, int type)
{
	unsigned char buf[51] = {};
	int fd = init_serial_port(dev_name);
	if (fd < 0) {
		return -1;
	} else {
		int len = do_read_from_serial(buf, fd);
		dump_str(buf, len);
	}
	clean_serial_port(fd);

	int t;
	if (type == 5) {
		t = (((unsigned int)buf[6])<<8) + (unsigned int)buf[7];
		return t;
	} else if(type == 10) {
		t = (((unsigned int)buf[8])<<8) + (unsigned int)buf[9];
		return t;
	} else
		return 0;
}

/*
 * Get datapoint data according to datapoint properties.
 * Here we fake the data using random numbers.
 */
void * get_datapoint_data(void *props)
{
	void *ret = NULL;
	const char *name = get_string_by_name(props, "name");
	ret = malloc(sizeof(double));
	if (ret == NULL) {
		perror("No memory available.\n");
		exit(-1);
	}

	if (strcmp(name, "temperature") == 0) {
		const char *mbd = get_string_by_name(props, "mbdev");
	    // 485 device address:3
	    // temperature register address: 1
		int val = get_mbsensor_data(mbd, 3, 1);
	    // convert to centigrade
	    double temperature = (double)val/10;
	    printf("Temperature is %2.2f C degree\n", temperature);

	    // return value to libsensor
	    *(double *)ret = (double)temperature;

	}else if (strcmp(name, "humidity") == 0) {
		const char *mbd = get_string_by_name(props, "mbdev");
	    // 485 device address:3
	    // humidity register address: 0
		int val = get_mbsensor_data(mbd, 3, 0);
	    // convert to centigrade
	    double humidity= (double)val/10;
	    printf("Humidity is %2.2f percent\n", humidity);
	    
	    // return value to libsensor
	    *(double *)ret = (double)humidity;

	} else if (strcmp(name, "pm2d5index") == 0) {
		const char *devn = get_string_by_name(props, "sdev");
	    // PM2.5
		int val = get_airpmindex(devn, 5);
	    // convert to ug/m3
	    double pm2d5= (double)val;
	    printf("PM2.5 is %2.2f ug/m3\n", pm2d5);
	    
	    // return value to libsensor
	    *(double *)ret = (double)pm2d5;

	} else if (strcmp(name, "pm10index") == 0) {
		const char *devn = get_string_by_name(props, "sdev");
	    // PM10
		int val = get_airpmindex(devn, 10);
	    // convert to ug/m3
	    double pm10= (double)val;
	    printf("PM10 is %2.2f ug/m3\n", pm10);
	    
	    // return value to libsensor
	    *(double *)ret = (double)pm10;

	} else if (strcmp(name, "light") == 0) {
		/* the light sensor's output connect to a2 pin of
		   galileo */
		int a2v = galileo_analog_read(2);
		/* the value of a2v should within [0,4096], that use 12bit to
		 * moderize 0~5v voltage, which means 0 stands for 0v while
		 * 4096 stands for 5v. */
		printf("Readed a2 pin voltage: %1.2f\n", ((double)a2v * 5) / 4096);

		/* then next we'll need to calculate temperature based on
		 * the design of temperature sensor.
		 */
		int val = a2v / 4;
		float light = (float)val;

		printf("The light is: %2.2f lux\n", light);

		/* return the temperature to libsensor */
		*(double *)ret = (double)light;

	} else if (strcmp(name, "image") == 0) {
		struct timeb t;
		ftime(&t);
		/* prepare a image file then return its name to libsensor */
		char *file = NULL;
		char *cmd = NULL;
		/* note, according to asprintf, the 'file' need be freed
		   later, which will be done by libsensor */
		asprintf(&file, "image_%lld%s", 1000 * (long long)t.time + t.millitm, ".jpg");
		asprintf(&cmd, "fswebcam -r 1280x720 --save %s 2>/dev/null", file);
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
