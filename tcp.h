#ifndef TCP_H
#define TCP_H

#include <pthread.h>
#include <sched.h>

#define MAX_CPUS 16

#define PORT 80

typedef struct {
	int epoll_fd;
} task_t;

int init_httpTCPServer(int);

int handle_write_post(void);

#endif
