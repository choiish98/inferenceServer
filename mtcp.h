#ifndef MTCP_H
#define MTCP_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <pthread.h>
#include <signal.h>

#include "mtcp_api.h"
#include "mtcp_epoll.h"

#include "cpu.h"
#include "http_parsing.h"
#include "netlib.h"
#include "debug.h"

#define MAX_CPUS 16

#define MAX_FLOW_NUM 10000
#define MAX_EVENTS MAX_FLOW_NUM * 3

#define RCVBUF_SIZE 2 * 1024
#define SNDBUF_SIZE 8 * 1024

#define HTTP_HEADER_LEN 1024
#define URL_LEN 128

#define true 1
#define false 0

// to be removed
#define NAME_LIMIT 256
#define FULLNAME_LIMIT 512
#define FILE_SIZE 15360

struct server_vars
{
    char request[HTTP_HEADER_LEN];
    int recv_len;
    int request_len;
    long int total_read, total_sent;
    uint8_t done;
    uint8_t rspheader_sent;
    uint8_t keep_alive;

	// to be removed
    int fd;
    char fname[NAME_LIMIT];             // file name
    long int fsize;                 // file size
    char buf[FILE_SIZE];
    int rd;
};

struct thread_context
{
    mctx_t mctx;
    int ep;
    struct server_vars *svars;
};

int init_httpServer(int, char *);

#endif
