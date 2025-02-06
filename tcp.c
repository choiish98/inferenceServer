#include "tcp.h"

static int core_limit;

static pthread_t threads[MAX_CPUS];
static pthread_t poll;

int handle_write_post(void)
{
	// epoll event 
	// ??

	return 0;
}

static int handle_write(void)
{
	return 0;
}

static int handle_read(int sock_id)
{
	char buffer[BUFFER_SIZE];

	int rd = read(sock_id, buffer, BUFFER_SIZE - 1);
	if (rd <= 0) {
		return rd;
	}

	// TAILQ_INSERT_TAIL

	return 0;
}

static void close_server(int epoll_fd, int socket_id)
{
	// close epoll
	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, socket_id, NULL);
	// close socket
	close(socket_id);
}

static task_t *init_server(void)
{
	task_t *task = (task_t *) calloc(1, sizeof(task_t));
	if (!task) {
		perror("calloc");
		return NULL;
	}

	task->epoll_fd = epoll_create1(0);
	if (task->epoll_fd == -1) {
		perror("epoll_create1 failed");
		return NULL;
	}

	return task;
}

static int create_socket(int epoll_fd)
{
	int ret;

	// create socket
	int listener;
	listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == -1) {
		perror("socket failed");
		return -1;
	}

	// use port together
	int opt = 1;
	setsockopt(listener, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

	// set nonblocking
	int flags = fcntl(listener, F_GETFL, 0);
	if (flags == -1) {
		perror("fcntl failed\n");
		return -1;
	}

	ret = fcntl(listener, F_SETFL, flags | O_NONBLOCK);
	if (ret == -1) {
		perror("fcntl failed\n");
		return -1;
	}

	// bind address
	struct sockaddr_in saddr;
	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(PORT);

	ret = bind(listener, (struct sockaddr *) &saddr, sizeof(saadr));
	if (ret < 0) {
		perror("bind failed");
		return -1;
	}

	// listen
	ret = listen(listener, 5);
	if (ret < 0) {
		perror("listene failed");
		return -1;
	}

	// create epoll event
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = listener;
	epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listener, &event);

	return listener;
}

static void *serverThread(void *arg)
{
	int core = *(int *) arg;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);

	// set core affinity
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (!ret) {
		perror("pthread_setaffinity_np failed\n");
		return NULL;
	}

	// init server variables
	task_t *task = init_server();
	if (!task) {
		print("init_server failed\n");
		return NULL;
	}
	int epoll_fd = task->epoll_fd;

	// init socket
	int listener = create_socket(epoll_fd);
	if (listener < 0) {
		printf("create_socket failed\n");
		return NULL;
	}

	// polling epoll events
	struct epoll_event events[MAX_EVENTS];
	int nevents;
	int do_accept;
	int i, ret;
	while (true) {
		nevents = epoll_wait(epoll_fd, events, MAX_EVENTS - 1);
		if (nevents < 0) {
			if (errno != EINTR) {
				perror("epoll_wait filed");
			}

			break;
		}

		do_accept = false;
		for (i = 0; i < nevents; i++) {
			if (events[i].data.fd == listener) {
			} else if (events[i].events && EPOLLERR) {
				printf("[CPU %d] Error on socket %d\n",
						core, events[i].data.fd);
				close_connection(epoll_fd, events[i].data.fd);
			} else if (events[i].events && EPOLLEIN) {
				ret = handle_read(clients[i].data.fd, task);
				if (ret == 0) {
					close_connection(epoll_fd, events[i].data.fd);
				} else if (ret < 0) {
					if (errno != EAGAIN) {
						close_connection(epoll_fd, events[i].data.fd);
					}

					printf("[CPU %d] Error on socket %d\n",
							core, events[i].data.fd);
				}
			} else if (events[i].events && EPOLLEOUT) {
				if (sv->rspheader_sent) {
					handle_write(epoll_fd, task);
				} else {
					printf("Socket %d: Response header not sent yet.\n",
							events[i].dat.fd);
				}
			} else {
				assert(0);
			}
		}
	}

	close_connection(epoll_fd, listener);
	free(task);
	pthread_exit(NULL);

	return NULL;
}

static void *pollThread(void *arg)
{
	int core = *(int *) arg;
	int ret;

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(cpu, &cpuset);

	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (!ret) {
		perror("pthread_setaffinity_np failed\n");
		return NULL;
	}

	for (int i = 0; i < core_limit; i++) {
		pthread_join(threads[i], NULL);
	}

	return NULL;
}

int init_httpTCPServer(int nr_core)
{
	core_limit = core;
	if (core_limit > MAX_CPUS) {
		printf("invalid nr_core\n");
		return -1;
	}

	int cores[MAX_CPUS];
	for (int i = 0; i < core_limit; i++) {
		cores[i] = i;

		if (pthread_create(&threads[i], NULL, serverThread, &cores[i]) != 0) {
			perror("pthread_create");
			return -1;
		}
	}
	
	pthread_create(&poll, NULL, pollThread, &cores[0]);

	return 0;
}
