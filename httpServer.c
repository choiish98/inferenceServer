#include "httpServer.h"

static pthread_t threads[MAX_CPUS];

static void close_flow(int ep, int socket_id)
{
	// close epoll
	epoll_ctl(ep, EPOLL_CTL_DEL, socket_id, NULL);
	// close socket
	close(socket_id);
}

int handle_accept(int listener, struct flow_context *ctx)
{
	// accept connection request
    int c = accept(listener, NULL, NULL);
    if (c >= 0) {
        if (c >= MAX_FLOW_NUM) {
            printf("Invalid socket id %d\n", c);
            return -1;
        }

		// set nonblocking
		int flags = fcntl(c, F_GETFL, 0);
		if (flags == -1) {
			perror("fcntl failed\n");
			return -1;
		}

		int ret = fcntl(c, F_SETFL, flags | O_NONBLOCK);
		if (ret == -1) {
			perror("fcntl failed\n");
			return -1;
		}

		// epollin event
        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = c;
        epoll_ctl(ctx->ep, EPOLL_CTL_ADD, c, &event);
    } else {
        if (errno != EAGAIN) {
            perror("Accept failed");
        }
    }

    return c;
}

static int handle_write(int sock_id, 
		struct flow_context *ctx, struct server_vars *sv)
{
	// rsp header
	time_t t_now;
	char t_str[128];
	time(&t_now);
	strftime(t_str, 128, "%a, %d %b %Y %X GMT", gmtime(&t_now));

	char keepalive_str[128];
	if (sv->keep_alive) {
		sprintf(keepalive_str, "Keep-Alive");
	} else {
		sprintf(keepalive_str, "Close");
	}

	char response[HTTP_HEADER_LEN];
	sprintf(response, "HTTP/1.1 200 OK\r\n"
			"Data: %s\r\n"
			"Server: Webserver on MiddleBox TCP (Ubuntu)\r\n"
			"Connection: %s\r\n"
			"\r\n%s",
			t_str, keepalive_str, sv->result);

	// write
	if (write(sock_id, response, strlen(response)) < 0) {
		perror("write failed");
		return -1;
	}

	if (!sv->keep_alive) {
		close_flow(ctx->ep, sock_id);
	}

	return 0;
}

static int handle_read(int sock_id, 
		struct flow_context *ctx, struct server_vars *sv)
{
	int rd = read(sock_id, sv->buf, BUFFER_SIZE - 1);
	if (rd <= 0) {
		perror("read failed\n");
		return rd;
	}

	sv->buf[rd] = '\0';
	printf("Received HTTP request:\n%s\n", sv->buf);

	// HTTP processing
	char *json = strstr(sv->buf, "\r\n\r\n");
	if (!json) {
		printf("Invalid HTTP request: No JSON payload\n");
		return -1;
	}
	json += 4;

	// SQ init
	request_t *req = (request_t *) malloc(sizeof(request_t));
	if (!req) {
		perror("malloc failed");
		return -1;
	}

	req->sock_id = sock_id;
	req->ep = ctx->ep;

	// JSON parsing
	sv->object = json_tokener_parse(json);
	if (!sv->object) {
		printf("json_tokener_pars failed\n");
		return -1;
	}

	if (json_object_object_get_ex(sv->object, "model_name", &sv->model)) {
		strncpy(req->model, json_object_get_string(sv->model), MODELNAME_SIZE);
		req->model[MODELNAME_SIZE - 1] = '\0';
	} else {
		printf("Missing 'model_name' in JSON\n");
		return -1;
	}

	if (json_object_object_get_ex(sv->object, "image_data", &sv->image)) {
		strncpy(req->image, json_object_get_string(sv->image), IMAGE_SIZE);
		req->image[IMAGE_SIZE - 1] = '\0';
	} else {
		printf("Missing 'image_data' in JSON\n");
		return -1;
	}

	// SQ insert
	TAILQ_INSERT_TAIL(&ctx->rm->sq, req, req_entries);

	return 0;
}

static struct flow_context *init_server(request_manager *rm)
{
	struct flow_context *ctx = (struct flow_context *) 
		calloc(1, sizeof(struct flow_context));
	if (!ctx) {
		perror("calloc");
		return NULL;
	}

	// create epoll
	ctx->ep = epoll_create1(0);
	if (ctx->ep == -1) {
		perror("epoll_create1 failed");
		return NULL;
	}

	ctx->rm = rm;

	return ctx;
}

static int create_socket(int ep)
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
	saddr.sin_port = htons(PORT);
	//saddr.sin_addr.s_addr = INADDR_ANY;
	if (inet_pton(AF_INET, "10.1.2.51", &saddr.sin_addr) <= 0) {
		perror("inet_pton failed");
		exit(EXIT_FAILURE);
	}

	ret = bind(listener, (struct sockaddr *) &saddr, sizeof(saddr));
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

	// listener epoll event
	struct epoll_event event;
	event.events = EPOLLIN;
	event.data.fd = listener;
	epoll_ctl(ep, EPOLL_CTL_ADD, listener, &event);

	return listener;
}

static void *serverThread(void *arg)
{
	int i, ret;
	request_manager rm = *(request_manager *) arg;

	int core = rm.cpu;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	// set core affinity
	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (ret) {
		perror("pthread_setaffinity_np failed\n");
		return NULL;
	}

	// init server variables
	struct flow_context *ctx = init_server(&rm);
	if (!ctx) {
		printf("init_server failed\n");
		return NULL;
	}
	int ep = ctx->ep;

	// init socket
	int listener = create_socket(ep);
	if (listener < 0) {
		printf("create_socket failed\n");
		return NULL;
	}

	printf("Server is listening on port %d..\n", PORT);

	// polling epoll events
	struct epoll_event events[MAX_EVENTS];
	int nevents;
	int do_accept;
	while (true) {
		nevents = epoll_wait(ep, events, MAX_EVENTS, -1);
		if (nevents < 0) {
			if (errno != EINTR) {
				perror("epoll_wait filed");
			}

			break;
		}

		do_accept = false;
		for (i = 0; i < nevents; i++) {
			printf("event %d\n", i);
			if (events[i].data.fd == listener) {
				do_accept = true;
			} else if (events[i].events && EPOLLERR) {
				printf("[CPU %d] Error on socket %d\n",
						core, events[i].data.fd);
				close_flow(ep, events[i].data.fd);
			} else if (events[i].events && EPOLLIN) {
				ret = handle_read(events[i].data.fd, ctx, 
						&ctx->svars[events[i].data.fd]);
				if (ret == 0) {
					close_flow(ep, events[i].data.fd);
				} else if (ret < 0) {
					if (errno != EAGAIN) {
						close_flow(ep, events[i].data.fd);
					}

					printf("[CPU %d] Error on socket %d\n",
							core, events[i].data.fd);
				}
			} else if (events[i].events && EPOLLOUT) {
				if (true) {
					handle_write(ep, ctx, &ctx->svars[events[i].data.fd]);
				} else {
					printf("Socket %d: Response header not sent yet.\n",
							events[i].data.fd);
				}
			} else {
				assert(0);
			}
		}

        if (do_accept) {
			while (1) {
				ret = handle_accept(listener, ctx);
				if (ret < 0)
					break;
			}
		}
	}

	close_flow(ep, listener);
	free(ctx);
	pthread_exit(NULL);

	return NULL;
}

int init_httpServer(int cpu, int core_limit, request_manager *rm)
{
	int ret;

	if (core_limit > MAX_CPUS) {
		printf("invalid nr_core\n");
		return -1;
	}

	for (int i = cpu; i < cpu + core_limit; i++) {
		ret = pthread_create(&threads[i], NULL, serverThread, &rm[i]);
		if (ret) {
			perror("pthread_create failed");
			return -1;
		}
	}

	return 0;
}

void poll_httpServer(int core, int core_limit)
{
	int ret;

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (ret) {
		perror("pthread_setaffinity_np failed\n");
		return;
	}

	for (int i = core; i < core_limit; i++) {
		pthread_join(threads[i], NULL);
	}
}

