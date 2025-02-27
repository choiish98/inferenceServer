#include "httpServer.h"

#include "encoder.h"

void close_connection(int ep, int socket_id)
{
	close(socket_id);
}

static void set_nonblocking(int socket)
{
	int flags = fcntl(socket, F_GETFL, 0);
	if (flags == -1) {
		perror("fcntl failed\n");
		exit(EXIT_FAILURE);
	}
	if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) == -1) {
		perror("fcntl failed\n");
		exit(EXIT_FAILURE);
	}
}

int handle_accept(int listener, struct flow_context *ctx)
{
    int c = accept(listener, NULL, NULL);
    if (c >= 0) {
        if (c >= MAX_FLOW_NUM) {
            printf("Invalid socket id %d\n", c);
            return -1;
        }

        struct epoll_event event;
        event.events = EPOLLIN;
        event.data.fd = c;
		set_nonblocking(c);
        epoll_ctl(ctx->ep, EPOLL_CTL_ADD, c, &event);
    } else {
        if (errno != EAGAIN) {
            perror("Accept failed");
        }
    }

    return c;
}

int handle_write(int sock_id, struct flow_context *ctx, struct server_vars *sv)
{
	time_t t_now;
	char t_str[128];
	char response[HTTP_HEADER_LEN];
	int ret;

	time(&t_now);
	strftime(t_str, 128, "%a, %d %b %Y %X GMT", gmtime(&t_now));

	// json parsing
	ret = json_encode(sv->result);
	if (ret) {
		printf("json_encode failed\n");
		return -1;
	}

	// HTTP processing
	// ignore keep_alive
	sprintf(response, "HTTP/1.1 200 OK\r\n"
			"Data: %s\r\n"
			"Server: Webserver on MiddleBox TCP (Ubuntu)\r\n"
			"Connection: Close\r\n"
			"\r\n%s",
			t_str, sv->result);

	ret = write(sock_id, response, strlen(response));
	if (ret < 0) {
		perror("write failed");
		close_connection(ctx->ep, sock_id);
		return -1;
	}

	close_connection(ctx->ep, sock_id);

	return 0;
}

int handle_read(int sock_id, struct flow_context *ctx, 
		struct server_vars *sv, request_t *req)
{
	char *json;
	int ret;

	ret = read(sock_id, sv->buf, BUFFER_SIZE - 1);
	if (ret <= 0) {
		perror("read failed\n");
		return -1;
	}

	sv->buf[ret] = '\0';
//	printf("Received HTTP request:\n%s\n\n", sv->buf);

	// HTTP processing
	json = strstr(sv->buf, "\r\n\r\n");
	if (!json) {
		printf("Invalid HTTP request: No JSON payload\n");
		return -1;
	}
	json += 4;

	//printf("Received JSON object:\n");

	// JSON parsing
	ret = json_decode(json, req->model, req->image);
	if (ret) {
		printf("json_decode failed\n");
		return -1;
	}

	/*
	sv->object = json_tokener_parse(json);
	if (!sv->object) {
		printf("json_tokener_parse failed\n");
		return -1;
	}

	if (json_object_object_get_ex(sv->object, "model_name", &sv->model)) {
		strncpy(req->model, json_object_get_string(sv->model), MODELNAME_SIZE);
		req->model[MODELNAME_SIZE - 1] = '\0';
	} else {
		printf("Missing 'model_name' in JSON\n");
		return -1;
	}
	//printf("Model name:%s\n", req->model);

	if (json_object_object_get_ex(sv->object, "image_data", &sv->image)) {
		strncpy(req->image, json_object_get_string(sv->image), IMAGE_SIZE);
		req->image[IMAGE_SIZE - 1] = '\0';
	} else {
		printf("Missing 'image_data' in JSON\n");
		return -1;
	}
	//printf("Received JSON object:%s\n\n", req->image);
	*/

	req->sock_id = sock_id;
	req->ep = ctx->ep;
	req->result = sv->result;

	return 0;
}

struct flow_context *init_server(int cpu)
{
	struct flow_context *ctx; 

	ctx = (struct flow_context *) calloc(1, sizeof(struct flow_context));
	if (!ctx) {
		perror("calloc");
		return NULL;
	}

	ctx->ep = epoll_create1(0);
	if (ctx->ep == -1) {
		perror("epoll_create1 failed");
		return NULL;
	}

	return ctx;
}

int create_socket(int ep)
{
	int listener;
	struct sockaddr_in saddr;
	struct epoll_event event;
	int ret;

	listener = socket(AF_INET, SOCK_STREAM, 0);
	if (listener == -1) {
		perror("socket failed");
		return -1;
	}

	int opt = 1;
	setsockopt(listener, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

	set_nonblocking(listener);

	saddr.sin_family = AF_INET;
	saddr.sin_addr.s_addr = INADDR_ANY;
	saddr.sin_port = htons(PORT);

	ret = bind(listener, (struct sockaddr *) &saddr, sizeof(saddr));
	if (ret < 0) {
		perror("bind failed");
		return -1;
	}

	ret = listen(listener, 5);
	if (ret < 0) {
		perror("listene failed");
		return -1;
	}

	event.events = EPOLLIN;
	event.data.fd = listener;
	epoll_ctl(ep, EPOLL_CTL_ADD, listener, &event);

	return listener;
}

