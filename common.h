#ifndef COMMON_H
#define COMMON_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <malloc.h>
#include <time.h>
#include <pthread.h>
#include <sys/resource.h>
#include <sched.h>
#include <sys/queue.h>

#define true 1
#define false 0

#define MAX_CPUS 16

#define MODELNAME_SIZE 256
#define IMAGE_SIZE 4096
#define RESULT_SIZE 1024

TAILQ_HEAD(request_head, request_t);
TAILQ_HEAD(response_head, response_t);

typedef struct request_t
{
	int sock_id;
	int ep;

	char model[MODELNAME_SIZE];
	char image[IMAGE_SIZE];
	char result[RESULT_SIZE];

	TAILQ_ENTRY(request_t) req_entries;
} request_t;

typedef struct response_t
{
	int sock_id;
	int ep;

	char result[RESULT_SIZE];

	TAILQ_ENTRY(response_t) res_entries;
} response_t;

typedef struct request_manager
{
	int cpu;

	struct request_head sq;
	struct response_head cq;
} request_manager;

#endif
