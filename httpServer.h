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

#define PORT 8080

#define MAX_FLOW_NUM 10000
#define MAX_EVENTS MAX_FLOW_NUM * 3

#define BUFFER_SIZE 8192

#define HTTP_HEADER_LEN 1024

struct server_vars
{
	char buf[BUFFER_SIZE];
	char result[RESULT_SIZE];
};

struct flow_context
{
	int ep;
	struct server_vars svars[MAX_FLOW_NUM];
};

int handle_accept(int, struct flow_context *);
int handle_write(int, struct flow_context *, struct server_vars *);
int handle_read(int, struct flow_context *, struct server_vars *, request_t *);

struct flow_context *init_server(int);
int create_socket(int);
void close_connection(int, int);

#endif
