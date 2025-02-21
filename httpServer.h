#ifndef HTTPSERVER_H
#define HTTPSERVER_H

#include "common.h"

#include <errno.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <json-c/json.h>  // JSON parsing 

#define PORT 8080

#define MAX_FLOW_NUM 10000
#define MAX_EVENTS MAX_FLOW_NUM * 3

#define BUFFER_SIZE 8192

#define HTTP_HEADER_LEN 1024


struct server_vars
{
	int keep_alive;

	char buf[BUFFER_SIZE];

	struct json_object *object;
	struct json_object *model;
	struct json_object *image;

	char result[RESULT_SIZE];
};

struct flow_context
{
	int ep;
	struct server_vars svars[MAX_FLOW_NUM];

	request_manager *rm;
};

int init_httpServer(int, int, request_manager *);
void poll_httpServer(int, int);

#endif
