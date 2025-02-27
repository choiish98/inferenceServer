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

#include <openssl/bio.h>
#include <openssl/evp.h>

#define true 1
#define false 0

#define MAX_CPUS 16

#define MODELNAME_SIZE 256
#define IMAGE_SIZE 10485760
#define RESULT_SIZE 512

TAILQ_HEAD(sq, request_t);
TAILQ_HEAD(cq, request_t);

typedef struct request_t
{
	int sock_id;
	int ep;

	int size;
	char model[MODELNAME_SIZE];
	char image[IMAGE_SIZE];
	char *result;

	TAILQ_ENTRY(request_t) req_entries;
} request_t;

#endif
