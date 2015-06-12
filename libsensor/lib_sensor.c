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
#ifdef _MSC_VER
#include <Winsock2.h>
#include <windows.h>
#else
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#endif
#include <string.h>
#include <errno.h>
#include <sys/timeb.h>
#include <json-c/json.h>
#include <time.h>

#include "list.h"

#define __stringify_1(x)    #x
#define __stringify(x)      __stringify_1(x)

#define MAXLINE 256

#ifdef _MSC_VER
#define insane_free(ptr) { free(ptr); ptr = 0; }
int vasprintf(char **strp, const char *fmt, va_list ap)
{
	int r = -1, size = _vscprintf(fmt, ap);

	if ((size >= 0) && (size < INT_MAX))
	{
		*strp = (char *)malloc(size + 1); //+1 for null
		if (*strp)
		{
			r = vsnprintf(*strp, size + 1, fmt, ap);  //+1 for null
			if ((r < 0) || (r > size))
			{
				insane_free(*strp);
				r = -1;
			}
		}
	} else { *strp = 0; }

	return(r);
}
int asprintf(char **strp, const char *fmt, ...)
{
	int r;
	va_list ap;
	va_start(ap, fmt);
	r = vasprintf(strp, fmt, ap);
	va_end(ap);
	return(r);
}
#endif

static int s_running = 1;

json_object *datapoints, *config;
const char *config_file;
int fd;

/* The list of intervals to collect data for each datapoint */
LIST_HEAD(intervals);

struct interval {
	struct list_head list;
	long long t;
};

struct upload_info {
	char *host;
	char *file;
	char *url;
	int port;
	int retry;
};

long long get_system_time() {
	struct timeb t;
	ftime(&t);
	return 1000 * (long long)t.time + t.millitm;
}

void sig_handler(int signo)
{
	printf("Received signal %d", signo);
	s_running = 0;
}

static void *http_putfile(void *thread_param);

/*
 * Schedule file transfer into a thread
 */
static int doFileTransfer(int id, char *file)
{
	int ret = 0;
	struct upload_info *upinfo = malloc(sizeof(struct upload_info));
	upinfo->file = strdup(file);

	/* cloud server address */
	json_object *jo = json_object_object_get(config, "cloudserveraddr");
	if (jo == NULL)
		upinfo->host = strdup("cloud.easyiot.com.cn");
	else
		upinfo->host = strdup(json_object_get_string(jo));

	/* cloud server port */
	jo = json_object_object_get(config, "cloudserverport");
	if (jo == NULL)
		upinfo->port = 80;
	else
		upinfo->port = json_object_get_int(jo);
#ifdef _MSC_VER
	char * filename = strchr(file, '\\');
#else
	char * filename = strchr(file, '/');
#endif
	filename = filename ? filename + 1 : file;
	jo = json_object_object_get(config, "api");
	if (jo == NULL)
		asprintf(&upinfo->url, "/api/file/personal/%d/%s?apiKey=%s",
		id,
		filename,
		json_object_get_string(json_object_object_get(config, "apikey"))
		);
	else
		asprintf(&upinfo->url, "%s/%d/%s?apiKey=%s",
		json_object_get_string(jo),
		id,
		filename,
		json_object_get_string(json_object_object_get(config, "apikey"))
		);

	jo = json_object_object_get(config, "retry");
	if (jo == NULL)
		upinfo->retry = 5;
	else
		upinfo->retry = json_object_get_int(jo);

	if (upinfo->file == NULL || upinfo->host == NULL || upinfo->url == NULL) {
		printf("Out of memory!");
		ret = -1;
	}

#ifdef _MSC_VER
       HANDLE thread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)http_putfile, (void *)upinfo, 0, NULL);
#else
       pthread_t thr;
       if (pthread_create(&thr, NULL, http_putfile, (void*)upinfo) != 0) {
               printf("create upload thread failed");
               ret = -1;
       }
#endif

       return ret;
}

/*
 * Get datapoint data according to datapoint index
 */
static void * get_datapoint_data_dummy(void *props)
{
	return NULL;
}

void * (*__get_datapoint_data)(void *) = get_datapoint_data_dummy;

static void add_interval() {
	/* add data collect time stamp for new datapoint */
	long long t = get_system_time();
	struct interval *inter = malloc(sizeof(struct interval));
	memset(inter, 0, sizeof(*inter));
	inter->t = t;
	list_add_tail((struct list_head *)inter, &intervals);
}

static void remove_interval(int i) {
	/* remove data collect time stamp for new datapoint */
	long long t = get_system_time();
	struct interval *inter = malloc(sizeof(struct interval));
	memset(inter, 0, sizeof(*inter));
	inter->t = t;
	list_add_tail((struct list_head *)inter, &intervals);
}

void handle_message(json_object *req, json_object *res)
{
	/* New Request from agent */
	json_object *method = json_object_object_get(req, "method");
	json_object *params = json_object_object_get(req, "params");
	json_object *idx, *jo, *val;
	int i, n, dpid;
	const char *msg;

	if (method == NULL || params == NULL) {
		/* Message format error */
		json_object_object_add(res, "result", json_object_new_boolean(FALSE));
		return;
	}

	if (strcmp(json_object_get_string(method), "set") == 0) {
		/*
		 * Set a parameter of a datapoint.
		 * Message format:
		 * {
		 *   "method": "set",
		 *   "params": {"id": ${datapoint_id}, "node": ${path}, "value": ${new_value}},
		 *   "id": ${msgid}
		 * }
		 * Response format:
		 * {
		 *   "result": true/false,
		 *   "id": ${msgid}
		 * }
		 */
		int found = 0;
		/* which datapoint to operate by id */
		dpid = json_object_get_int(json_object_object_get(params, "id"));
		n = json_object_array_length(datapoints);
		for (i = 0; i < n; i++) {
			idx = json_object_array_get_idx(datapoints, i);
			if (atoi(json_object_get_string(json_object_object_get(idx, "id"))) == dpid)
				break;
		}
		if (i < n) {
			/* found datapoint id, try to find node */
			msg = json_object_get_string(json_object_object_get(params, "node"));
			jo = json_object_object_get(idx, "props");
			if (jo != NULL) {
				val = json_object_object_get(jo, msg);
				if (val != NULL) {
					/* node found, set value */
					json_object_object_add(jo, msg, json_object_get(json_object_object_get(params, "value")));
					found = 1;
				}
			}
		}
		if (found == 1) {
			/*
			 * return the "props" field of the datapoint to let the agent update
			 * the database and the in memory tree
			 */
			json_object_object_add(res, "result", json_object_get(json_object_object_get(idx, "props")));
			/* write config back to config file */
			json_object_to_file_ext(config_file, config, JSON_C_TO_STRING_PRETTY);
		} else {
			/* Config path not found, send response */
			json_object_object_add(res, "result", json_object_new_boolean(FALSE));
		}
	} else if (strcmp(json_object_get_string(method), "getData") == 0) {
		/*
		 * Get instant data of a datapoint.
		 * Message format:
		 * {
		 *   "method": "getData",
		 *   "params": "${datapoint_id}"
		 *   "id": ${msgid}
		 * }
		 * Response format:
		 * {
		 *   "result": ${data}/false,
		 *   "id": ${msgid}
		 * }
		 */

		dpid = atoi(json_object_get_string(params));

		// Try to find id from local managed tree
		n = json_object_array_length(datapoints);
		for (i = 0; i < n; i++) {
			idx = json_object_array_get_idx(datapoints, i);
			if (atoi(json_object_get_string(json_object_object_get(idx, "id"))) == dpid) break;
		}

		// See if found
		if (i < n) {
			json_object *data_obj = NULL;
			json_object *result_obj = NULL;
			const char *datatype = json_object_get_string(json_object_object_get(idx, "dataType"));
			void *data = __get_datapoint_data(json_object_object_get(idx, "props"));
			if (data != NULL) {
				if (strcmp(datatype, "numeric") == 0) {
					data_obj = json_object_new_double(*(double*)data);
				} else if (strcmp(datatype, "file") == 0) {
					if (doFileTransfer(dpid, (char*)data) == 0) {
						data_obj = json_object_new_string((const char*)data);
					} else {
						fprintf(stderr, "Upload file to server failed.\n");
					}
				}
				free(data);
			}
			if (data_obj != NULL) {
				result_obj = json_object_new_object();
				json_object_object_add(result_obj, "date", json_object_new_int64(get_system_time()));
				json_object_object_add(result_obj, "data", data_obj);
				json_object_object_add(res, "result", result_obj);
			} else {
				json_object_object_add(res, "result", json_object_new_boolean(FALSE));
				json_object_object_add(res, "error", json_object_new_string("Get data failed."));
			}
		} else {
			json_object_object_add(res, "result", json_object_new_boolean(FALSE));
			json_object_object_add(res, "error", json_object_new_string("ID not found!"));
		}
	} else if (strcmp(json_object_get_string(method), "add") == 0) {
		/*
		 * add a datapoint instant.
		 * Message format:
		 * {
		 *   "method": "add",
		 *   "params": {
		 *     ${datapoint_id_1}: ${datapoint_props},
		 *     ...
		 *     ${datapoint_id_N}: ${datapoint_props}
		 *   },
		 *   "id": ${msgid}
		 * }
		 * Response format:
		 * {
		 *   "result": true/false,
		 *   "id": ${msgid}
		 * }
		 */
		struct lh_table *dp_table = json_object_get_object(params);
		struct lh_entry *dp_entry;
		lh_foreach(dp_table, dp_entry) {
			add_interval();
			json_object *newdp = json_object_new_object();
			json_object_object_add(newdp, "id", json_object_new_string((char*)dp_entry->k));
			json_object_object_add(newdp, "props", json_object_get((struct json_object *)dp_entry->v));
			json_object_array_add(datapoints, json_object_get(newdp));
		}
		json_object_to_file_ext(config_file, config, JSON_C_TO_STRING_PRETTY);
		json_object_object_add(res, "result", json_object_new_boolean(TRUE));
	} else if (strcmp(json_object_get_string(method), "del") == 0) {
		/*
		 * Get instant data of a datapoint.
		 * Message format:
		 * {
		 *   "method": "del",
		 *   "params": ${datapoint_id},
		 *   "id": ${msgid}
		 * }
		 * Response format:
		 * {
		 *   "result": ${data}/false,
		 *   "id": ${msgid}
		 * }
		 */

		dpid = atoi(json_object_get_string(params));

		// Try to find id from local managed tree
		n = json_object_array_length(datapoints);
		json_object *new_dps = json_object_new_array();
		int found = -1;
		for (i = 0; i < n; i++) {
			idx = json_object_array_get_idx(datapoints, i);
			if (atoi(json_object_get_string(json_object_object_get(idx, "id"))) == dpid)
				found = i;
			else
				json_object_array_add(new_dps, json_object_get(idx));
		}

		// See if found
		if (found >= 0) {
			remove_interval(found);
			json_object_object_add(res, "result", json_object_new_boolean(TRUE));
			json_object_object_add(config, "datapoints", new_dps);
			datapoints = new_dps;
			json_object_to_file_ext(config_file, config, JSON_C_TO_STRING_PRETTY);
		} else {
			json_object_object_add(res, "result", json_object_new_boolean(FALSE));
			json_object_object_add(res, "error", json_object_new_string("ID not found!"));
		}
	} else {
		/* Other request, we don't support yet. */
		json_object_object_add(res, "result", json_object_new_boolean(FALSE));
	}
}

/*
 * Register datapoints to the agent
 * Message format:
 * {
 *   "method": "reg",
 *   "params": {
 *      "Name": "${appname}",
 *   	"New": [${new_datapoint_profile_1},
 *   		${new_datapoint_profile_2},
 *   		..,
 *   		${new_datapoint_profile_N}],
 *   	"Managed": [${managed_datapoint_id_1},
 *   		${managed_datapoint_id_2},
 *   		..,
 *   		${managed_datapoint_id_N}]
 *   },
 *   "id": ${msgid}
 * }
 */
void registerdatapoints(int msgid) {
	json_object *new_field = json_object_new_array();
	json_object *managed_field = json_object_new_array();

	int i = 0;
	int dp_num = json_object_array_length(datapoints);
	json_object *idx, *val;
	for (i = 0; i < dp_num; i++) {
		idx = json_object_array_get_idx(datapoints, i);

		add_interval();

		val = json_object_object_get(idx, "id");
		if (val == NULL) {
			json_object_array_add(new_field, json_object_get(json_object_object_get(idx, "props")));
		} else {
			json_object_array_add(managed_field, json_object_get(val));
		}
	}

	int bytes;
	json_object *reg_msg = json_object_new_object();
	val = json_object_new_object();

	json_object_object_add(reg_msg, "method", json_object_new_string("reg"));
	json_object_object_add(reg_msg, "id", json_object_new_int(msgid));
	if (json_object_array_length(new_field) != 0) {
		json_object_object_add(val, "New", new_field);
	}
	if (json_object_array_length(managed_field) != 0) {
		json_object_object_add(val, "Managed", managed_field);
	}
	json_object_object_add(val, "appName", json_object_get(json_object_object_get(config, "appName")));
	json_object_object_add(reg_msg, "params", val);
	const char *msg = json_object_get_string(reg_msg);
	if ((bytes = send(fd, msg, strlen(msg), 0)) < 0) {
		printf("write socket error:%s\n", strerror(errno));
		close(fd);
		exit(-1);
	}
	json_object_put(reg_msg);
}

int lib_sensor_start(const char *cfg_file,
	void *(*get_datapoint_data_func)(void *),
	void(*set_datapoint_func)(void *, void *),
	void *data
	)
{
	int ret;
	printf("lib_sensor-%s is initializing ...\n", __stringify(VERSION));
#ifdef _MSC_VER

	WORD wVersionRequested;
	WSADATA wsaData;
	int err;

	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		printf("WSAStartup failed witherror: %d\n", err);
		return 1;
	}
#endif
	if (cfg_file == NULL || get_datapoint_data_func == NULL) {
		printf("lib_sensor: wrong parameters.\n");
		return -1;
	}

	config_file = cfg_file;

	/*
	 *      Note, the 'set_datapoint_func' is optional, designed for futher extension
	 */
	__get_datapoint_data = get_datapoint_data_func;

	struct json_tokener *Json_tokener = json_tokener_new();
	if (!Json_tokener) {
		printf("Out of memory when json_tokener_new();");
		exit(-1);
	}

	/* Load and parse configuration file */
	config = json_object_from_file(config_file);
	if (config == NULL) {
		printf("Failed to parse configuration file: %s", config_file);
		return -1;
	}

	datapoints = json_object_object_get(config, "datapoints");
	if (json_object_get_type(datapoints) != json_type_array) {
		printf("sensor config error!");
		return -1;
	}

	/* try to connect to server */
	const char *host = json_object_get_string(json_object_object_get(config, "host"));
	int port = json_object_get_int(json_object_object_get(config, "port"));

	struct sockaddr_in sock;
	sock.sin_family = PF_INET;
	sock.sin_port = htons(port);
	sock.sin_addr.s_addr = inet_addr(host);
	fd = socket(AF_INET, SOCK_STREAM, 0);

	if (fd < 0) {
		printf("socket() failed error: %d\n", errno);
		return -1;
	}

	if (signal(SIGINT, sig_handler) == SIG_ERR
#ifdef SIGQUIT
		|| signal(SIGQUIT, sig_handler) == SIG_ERR
#endif
		) {
		printf("Can't register signal handler.");
	}

	while ((ret = connect(fd, (struct sockaddr*)&sock, sizeof(struct sockaddr_in))) == -1) {
		if (EINPROGRESS != errno) {
			printf("Can not connect to dmagent: %s\n", strerror(errno));
			return -1;
		}
	}

	/* register datapoints to agent */
	int n, i, bytes, msgid = 1;
	registerdatapoints(msgid);

	printf("sensor app successfully connected to agent.\n");

	/* handling retriving messages/commands */
	fd_set fdset;
	int max_read_size, pos = 0;
	unsigned int buffer_size = 1024;
	char *buffer = malloc(buffer_size);
	memset(buffer, 0, buffer_size);
	long long t;
	struct interval *inter;
	char *msg;
	json_object *idx, *jo, *val, *res;

	while (s_running) {
		struct timeval timeout = { 0, 10000 };
		FD_ZERO(&fdset);
		FD_SET(fd, &fdset);
		ret = select(fd + 1, &fdset, NULL, NULL, &timeout);
		if (ret == -1) {
			printf("Server-select() error!\n");
			return -1;
		} else if (ret == 0) {
			/* nothing to receive. check if we have data to send */
			n = json_object_array_length(datapoints);
			struct list_head *next = intervals.next;
			int opt;
			t = get_system_time();
			for (i = 0; i < n; i++) {
				idx = json_object_array_get_idx(datapoints, i);
				opt = atoi(json_object_get_string(json_object_object_get(json_object_object_get(idx, "props"), "sampleRate")));
				inter = list_entry(next, struct interval, list);
				/* check if it is time to collect datapoint data */
				if (t - inter->t > opt * 1000) {
					msg = NULL;
					srand(time(NULL));

					/*
					 * Send datapoint data to the agent
					 * Message format:
					 * {
					 *   "method": "data",
					 *   "params": {
					 *     "${datapoint_id}": {
					 *       "date": ${date},
					 *       "data":"${data}"
					 *     }
					 *   },
					 *   "id":${msgid}
					 * }
					 * Here we just use strings instead of json_object to send out the message.
					 */
					json_object *props = json_object_object_get(idx, "props");
					const char *datatype = json_object_get_string(json_object_object_get(props, "dataType"));
					void *data = __get_datapoint_data(props);
					const char *id = json_object_get_string(json_object_object_get(idx, "id"));
					if (data != NULL) {
						if (strcmp(datatype, "numeric") == 0) {
							asprintf(&msg, "{\"method\": \"data\", \"params\":{\"%s\": {\"date\":%lld, \"data\":\"%lf\"}}, \"id\":%lld}",
								id, t, *(double*)data, t);
						} else if (strcmp(datatype, "file") == 0) {
							if (doFileTransfer(atoi(id), (char*)data) == 0) {
								asprintf(&msg, "{\"method\": \"data\", \"params\":{\"%s\": {\"date\":%lld, \"data\":\"%s\"}}, \"id\":%lld}",
								         id, t, (char*)data, t);
							} else {
								fprintf(stderr, "upload file to server failed.\n");
							}
						}
						free(data);
					}
					if (msg) {
						printf("sending server data msg: %s\n", msg);
						if ((bytes = send(fd, msg, strlen(msg), 0)) < 0) {
							perror("write socket error!");
							close(fd);
							s_running = 0;
						}
						free(msg);
					}
					inter->t = t;
				}
				next = next->next;
			}
		} else {
			/* data available, try to receive all buffer */
			do {
				if (pos == (buffer_size - 1)) {
					msg = realloc(buffer, buffer_size *= 2);
					if (msg == NULL) {
						printf("Out of memory when allocating buffer!");
						close(fd);
						exit(-1);
						return 1;
					}
					buffer = msg;
					memset(buffer + pos, 0, buffer_size - pos);
				}
				// can not fill the entire buffer, string must be NULL terminated
				max_read_size = buffer_size - pos - 1;
				bytes = recv(fd, buffer + pos, max_read_size, 0);
				if (bytes < 0) {
					perror("read");
					close(fd);
					return 1;
				}
				pos += bytes;
			} while (bytes == max_read_size);

			printf("message received:%s\n", buffer);

			/*
			 * Message could be operation result or new request.
			 */
			json_tokener_reset(Json_tokener);
			jo = json_tokener_parse_ex(Json_tokener, buffer, strlen(buffer));
			val = json_object_object_get(jo, "error");
			if (val == NULL) {
				val = json_object_object_get(jo, "result");
				if (val != NULL) {
					/* Response */
					if (msgid == json_object_get_int(json_object_object_get(jo, "id"))) {
						/* reg success
						 * If the result is an array of ids, update local config file.
						 */
						if (json_object_get_type(val) == json_type_array) {
							n = json_object_array_length(val);
							for (i = 0; i < n; i++) {
								idx = json_object_array_get_idx(val, i);
								json_object_object_add(json_object_array_get_idx(datapoints, i), "id", json_object_get(idx));
							}
							/* write config back to config file */
							json_object_to_file_ext(config_file, config, JSON_C_TO_STRING_PRETTY);
						}
					}
				} else {
					/* New Request */
					res = json_object_new_object();
					handle_message(jo, res);

					/* Send request processed result back */
					const char *sendbuf = json_object_get_string(res);
					if ((bytes = send(fd, sendbuf, strlen(sendbuf), 0)) < 0) {
						perror("write socket error!");
						close(fd);
						s_running = 0;
					}
					json_object_put(res);
				}
			} else {
				printf("Message failed! Result:%s", json_object_get_string(val));
			}

			/* Message process finished, reset state for next message */
			json_object_put(jo);
			pos = 0;
			memset(buffer, 0, buffer_size);
		}
	}
#ifdef _MSC_VER
	WSACleanup();
#endif
	return 0;
}

void * get_node_by_name(void *pnode, const char *name)
{
	if (pnode == NULL || name == NULL)
		return NULL;

	return json_object_object_get((json_object *)pnode, name);
}

int get_int_by_name(void *node, const char *name)
{
	json_object *js_node = NULL;

	js_node = json_object_object_get((json_object *)node, name);
	return json_object_get_int(js_node);
}

double get_double_by_name(void *node, const char *name)
{
	json_object *js_node = NULL;

	js_node = json_object_object_get((json_object *)node, name);
	return json_object_get_double(js_node);
}

const char *get_string_by_name(void *node, const char *name)
{
	json_object *js_node = NULL;

	js_node = json_object_object_get((json_object *)node, name);
	return json_object_get_string(js_node);
}

int int_from_config_by_name(const char *name)
{
	return json_object_get_int(json_object_object_get(config, name));
}

const char *string_from_config_by_name(const char *name)
{
	return json_object_get_string(json_object_object_get(config, name));
}

static void *http_putfile(void *thread_param)
{
	if (thread_param == NULL)
		return NULL;

#ifndef _MSC_VER
	pthread_detach(pthread_self());
#endif

	FILE *fp;
	struct hostent *hptr;
	struct sockaddr_in servaddr;
	struct upload_info *upinfo = (struct upload_info *)thread_param;
	char buf[256] = {};
	int i, sockfd, size;
	int n;
	char *sendline = NULL;
	char recvline[MAXLINE + 1];
	int ret = 0;

	/* get size of the file */
	fp = fopen(upinfo->file, "r");
	if (fp == NULL) {
		printf("Fail to read file: %s\n", upinfo->file);
		goto cleanup;
	}
	ret = fseek(fp, 0L, SEEK_END);
	if (ret < 0) {
		fprintf(stderr, "fseek error: %d\n", errno);
		goto cleanup;
	}
	size = ftell(fp);
	if (size < 0) {
		fprintf(stderr, "ftell error: %d\n", errno);
		goto cleanup;
	}

	if ((hptr = gethostbyname(upinfo->host)) == NULL) {
		printf("gethostbyname error for host: %s\n", upinfo->host);
		goto cleanup;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd < 0) {
		fprintf(stderr, "create socket error: %d\n", errno);
		goto cleanup;
	}

	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(upinfo->port);
	memcpy(&(servaddr.sin_addr.s_addr), hptr->h_addr, hptr->h_length);

	for (i = 0; i < upinfo->retry; i++) {
		while (connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
			if (EINPROGRESS != errno) {
				fprintf(stderr, "connect to server error: %d\n", errno);
			}
		}

		ret = fseek(fp, 0L, SEEK_SET);
		if (ret < 0) {
			fprintf(stderr, "feek error #1: %d\n", errno);
			goto cleanup;
		}

		if (sendline != NULL) {
			free(sendline);
			sendline = NULL;
		}

		ret = asprintf(&sendline,
			"PUT %s HTTP/1.1\r\n"
			"HOST: %s\r\n"
			"Content-type: application/octet-stream\r\n"
			"Content-length: %d\r\n\r\n"
			, upinfo->url, upinfo->host, size);
		if (ret < 0) {
			fprintf(stderr, "memory exhausted.\n");
			continue;
		}

		if ((n = send(sockfd, sendline, strlen(sendline), 0)) > 0) {
			while (fread(buf, 1, sizeof(buf), fp) > 0) {
				if ((n = send(sockfd, buf, sizeof(buf), 0)) <= 0)
					break;
			}
		}

		while ((n = recv(sockfd, recvline, MAXLINE, 0)) > 0) {
#if 0
			/* dump out received header */
			recvline[n] = '\0';
			printf("%s", recvline);
#endif
			if (strstr(recvline, "204 No Content") != NULL) {
				fflush(stdout);
				ret = 0;
				i = upinfo->retry;
				close(sockfd);
				sockfd = -1;
				goto cleanup;
			}
		}
	}

cleanup:
	if (sockfd > 0)
		close(sockfd);

	if (fp != NULL)
		fclose(fp);

	remove(upinfo->file);
	if (upinfo->host != NULL)
		free(upinfo->host);
	if (upinfo->url != NULL)
		free(upinfo->url);
	if (upinfo->file != NULL)
		free(upinfo->file);

	free(upinfo);
	printf("--- finished file upload ---\n");

	return NULL;
}

