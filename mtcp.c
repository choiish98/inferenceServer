#include "mtcp.h"

static int core_limit;
static int done[MAX_CPUS];
static pthread_t app_thread[MAX_CPUS];
static pthread_t poll_thread;

// to be removed
const char *www_main;
static int backlog = -1;

static char *StatusCodeToString(int scode)
{
    switch (scode) {
        case 200:
            return "OK";
            break;

        case 404:
            return "Not Found";
            break;
    }

    return NULL;
}

static void CleanServerVariable(struct server_vars *sv)
{
    sv->recv_len = 0;
    sv->request_len = 0;
    sv->total_read = 0;
    sv->total_sent = 0;
    sv->done = 0;
    sv->rspheader_sent = 0;
    sv->keep_alive = 0;
}

static void
CloseConnection(struct thread_context *ctx, int sockid, struct server_vars *sv)
{
    mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_DEL, sockid, NULL);
    mtcp_close(ctx->mctx, sockid);
}

static int
SendUntilAvailable(struct thread_context *ctx, int sockid, struct server_vars *sv)
{
    int ret;
    int sent;
    int len;

    if (sv->done || !sv->rspheader_sent) {
        return 0;
    }

    sent = 0;
    ret = 1;
    while (ret > 0) {
        len = MIN(SNDBUF_SIZE, sv->fsize - sv->total_sent);
        if (len <= 0) {
            break;
        }
        ret = mtcp_write(ctx->mctx, sockid, sv->buf + sv->total_sent, len);
        if (ret < 0) {
            TRACE_APP("Connection closed with client.\n");
            break;
        }
        TRACE_APP("Socket %d: mtcp_write try: %d, ret: %d\n", sockid, len, ret);
        sent += ret;
        sv->total_sent += ret;
    }

    if (sv->total_sent >= sv->fsize) {
        struct mtcp_epoll_event ev;
        sv->done = TRUE;

        if (sv->keep_alive) {
            /* if keep-alive connection, wait for the incoming request */
            ev.events = MTCP_EPOLLIN;
            ev.data.sockid = sockid;
            mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);

            CleanServerVariable(sv);
        } else {
            /* else, close connection */
            CloseConnection(ctx, sockid, sv);
        }
    }

    return sent;
}

static int
HandleReadEvent(struct thread_context *ctx, int sockid, struct server_vars *sv)
{
    struct mtcp_epoll_event ev;
    char buf[HTTP_HEADER_LEN];
    char url[URL_LEN];
    char response[HTTP_HEADER_LEN];
    int scode = 200;                      // status code
    time_t t_now;
    char t_str[128];
    char keepalive_str[128];
    int rd;
    int len;
    int sent;

    /* HTTP request handling */
    rd = mtcp_read(ctx->mctx, sockid, buf, HTTP_HEADER_LEN);
    if (rd <= 0) {
        return rd;
    }
    memcpy(sv->request + sv->recv_len,
            (char *)buf, MIN(rd, HTTP_HEADER_LEN - sv->recv_len));
    sv->recv_len += rd;
    //sv->request[rd] = '\0';
    //fprintf(stderr, "HTTP Request: \n%s", request);
    sv->request_len = find_http_header(sv->request, sv->recv_len);
    if (sv->request_len <= 0) {
        TRACE_ERROR("Socket %d: Failed to parse HTTP request header.\n"
                "read bytes: %d, recv_len: %d, "
                "request_len: %d, strlen: %ld, request: \n%s\n",
                sockid, rd, sv->recv_len,
                sv->request_len, strlen(sv->request), sv->request);
        return rd;
    }

    http_get_url(sv->request, sv->request_len, url, URL_LEN);
    TRACE_APP("Socket %d URL: %s\n", sockid, url);
    sprintf(sv->fname, "%s%s", www_main, url);
    TRACE_APP("Socket %d File name: %s\n", sockid, sv->fname);

    sv->keep_alive = FALSE;
    if (http_header_str_val(sv->request, "Connection: ",
                strlen("Connection: "), keepalive_str, 128)) {
        if (strstr(keepalive_str, "Keep-Alive")) {
            sv->keep_alive = TRUE;
        } else if (strstr(keepalive_str, "Close")) {
            sv->keep_alive = FALSE;
        }
    }

    /* Response header handling */
    time(&t_now);
    strftime(t_str, 128, "%a, %d %b %Y %X GMT", gmtime(&t_now));
    if (sv->keep_alive)
        sprintf(keepalive_str, "Keep-Alive");
    else
        sprintf(keepalive_str, "Close");

    sprintf(response, "HTTP/1.1 %d %s\r\n"
            "Date: %s\r\n"
            "Server: Webserver on Middlebox TCP (Ubuntu)\r\n"
            "Content-Length: %ld\r\n"
            "Connection: %s\r\n\r\n",
            scode, StatusCodeToString(scode), t_str, sv->fsize, keepalive_str);
    len = strlen(response);
    TRACE_APP("Socket %d HTTP Response: \n%s", sockid, response);
    sent = mtcp_write(ctx->mctx, sockid, response, len);
    TRACE_APP("Socket %d Sent response header: try: %d, sent: %d\n",
            sockid, len, sent);
    assert(sent == len);
    sv->rspheader_sent = TRUE;

    ev.events = MTCP_EPOLLIN | MTCP_EPOLLOUT;
    ev.data.sockid = sockid;
    mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_MOD, sockid, &ev);

    SendUntilAvailable(ctx, sockid, sv);

    return rd;
}

static int AcceptConnection(struct thread_context *ctx, int listener)
{
    mctx_t mctx = ctx->mctx;
    struct server_vars *sv;
    struct mtcp_epoll_event ev;
    int c;

    c = mtcp_accept(mctx, listener, NULL, NULL);

    if (c >= 0) {
        if (c >= MAX_FLOW_NUM) {
            TRACE_ERROR("Invalid socket id %d.\n", c);
            return -1;
        }

        sv = &ctx->svars[c];
        CleanServerVariable(sv);
        TRACE_APP("New connection %d accepted.\n", c);
        ev.events = MTCP_EPOLLIN;
        ev.data.sockid = c;
        mtcp_setsock_nonblock(ctx->mctx, c);
        mtcp_epoll_ctl(mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, c, &ev);
        TRACE_APP("Socket %d registered.\n", c);

    } else {
        if (errno != EAGAIN) {
            TRACE_ERROR("mtcp_accept() error %s\n",
                    strerror(errno));
        }
    }

    return c;
}

static struct thread_context *InitializeServerThread(int core)
{
    struct thread_context *ctx;

    /* affinitize application thread to a CPU core */
    mtcp_core_affinitize(core);

    ctx = (struct thread_context *)calloc(1, sizeof(struct thread_context));
    if (!ctx) {
        TRACE_ERROR("Failed to create thread context!\n");
        return NULL;
    }

    /* create mtcp context: this will spawn an mtcp thread */
    ctx->mctx = mtcp_create_context(core);
    if (!ctx->mctx) {
        TRACE_ERROR("Failed to create mtcp context!\n");
        free(ctx);
        return NULL;
    }

    /* create epoll descriptor */
    ctx->ep = mtcp_epoll_create(ctx->mctx, MAX_EVENTS);
    if (ctx->ep < 0) {
        mtcp_destroy_context(ctx->mctx);
        free(ctx);
        TRACE_ERROR("Failed to create epoll descriptor!\n");
        return NULL;
    }

    /* allocate memory for server variables */
    ctx->svars = (struct server_vars *)
            calloc(MAX_FLOW_NUM, sizeof(struct server_vars));
    if (!ctx->svars) {
        mtcp_close(ctx->mctx, ctx->ep);
        mtcp_destroy_context(ctx->mctx);
        free(ctx);
        TRACE_ERROR("Failed to create server_vars struct!\n");
        return NULL;
    }

    return ctx;
}

static int CreateListeningSocket(struct thread_context *ctx)
{
    int listener;
    struct mtcp_epoll_event ev;
    struct sockaddr_in saddr;
    int ret;

    /* create socket and set it as nonblocking */
    listener = mtcp_socket(ctx->mctx, AF_INET, SOCK_STREAM, 0);
    if (listener < 0) {
        TRACE_ERROR("Failed to create listening socket!\n");
        return -1;
    }
    ret = mtcp_setsock_nonblock(ctx->mctx, listener);
    if (ret < 0) {
        TRACE_ERROR("Failed to set socket in nonblocking mode.\n");
        return -1;
    }

    /* bind to port 80 */
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = INADDR_ANY;
    saddr.sin_port = htons(80);
    ret = mtcp_bind(ctx->mctx, listener,
            (struct sockaddr *)&saddr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        TRACE_ERROR("Failed to bind to the listening socket!\n");
        return -1;
    }

    /* listen (backlog: can be configured) */
    ret = mtcp_listen(ctx->mctx, listener, backlog);
    if (ret < 0) {
        TRACE_ERROR("mtcp_listen() failed!\n");
        return -1;
    }

    /* wait for incoming accept events */
    ev.events = MTCP_EPOLLIN;
    ev.data.sockid = listener;
    mtcp_epoll_ctl(ctx->mctx, ctx->ep, MTCP_EPOLL_CTL_ADD, listener, &ev);

    return listener;
}

static void *RunServerThread(void *arg)
{
	int core = *(int *) arg;
	struct thread_context *ctx;

	ctx = InitializeServerThread(core);
	if (!ctx) {
		printf("InitializeServerThread failed\n");
		return NULL;
	}
	mctx_t mctx = ctx->mctx;
	int ep = ctx->ep;

	struct mtcp_epoll_event *events = (struct mtcp_epoll_event *)
		calloc(MAX_EVENTS, sizeof(struct mtcp_epoll_event));
	if (!events) {
		printf("calloc failed\n");
		exit(-1);
	}

	int listener = CreateListeningSocket(ctx);
	if (listener < 0) {
		printf("CreateListeningSocket failed\n");
		exit(-1);
	}

	while (!done[core]) {
		int nevents = mtcp_epoll_wait(mctx, ep, events, MAX_EVENTS, -1);
		if (nevents < 0) {
			if (errno != EINTR) {
				perror("mtcp_epoll_wait");
			}

			break;
		}

		int do_accept = FALSE;
		for (int i = 0; i < nevents; i++) {
			if (events[i].data.sockid == listener) {
                /* if the event is for the listener, accept connection */
                do_accept = TRUE;
            } else if (events[i].events & MTCP_EPOLLERR) {
                int err;
                socklen_t len = sizeof(err);

                /* error on the connection */
                TRACE_APP("[CPU %d] Error on socket %d\n",
                        core, events[i].data.sockid);
                if (mtcp_getsockopt(mctx, events[i].data.sockid,
                        SOL_SOCKET, SO_ERROR, (void *)&err, &len) == 0) {
                    if (err != ETIMEDOUT) {
                        fprintf(stderr, "Error on socket %d: %s\n",
                                events[i].data.sockid, strerror(err));
                    }
                } else {
                    perror("mtcp_getsockopt");
                }
                CloseConnection(ctx, events[i].data.sockid,
                        &ctx->svars[events[i].data.sockid]);

            } else if (events[i].events & MTCP_EPOLLIN) {
                int ret = HandleReadEvent(ctx, events[i].data.sockid,
                        &ctx->svars[events[i].data.sockid]);

                if (ret == 0) {
                    /* connection closed by remote host */
                    CloseConnection(ctx, events[i].data.sockid,
                            &ctx->svars[events[i].data.sockid]);
                } else if (ret < 0) {
                    /* if not EAGAIN, it's an error */
                    if (errno != EAGAIN) {
                        CloseConnection(ctx, events[i].data.sockid,
                                &ctx->svars[events[i].data.sockid]);
                    }
                }

            } else if (events[i].events & MTCP_EPOLLOUT) {
                struct server_vars *sv = &ctx->svars[events[i].data.sockid];
                if (sv->rspheader_sent) {
                    SendUntilAvailable(ctx, events[i].data.sockid, sv);
                } else {
                    TRACE_APP("Socket %d: Response header not sent yet.\n",
                            events[i].data.sockid);
                }

            } else {
                assert(0);
            }
		}

		/* if do_accept flag is set, accept connections */
		if (do_accept) {
			while (1) {
				int ret = AcceptConnection(ctx, listener);
				if (ret < 0)
					break;
			}
		}
	}

	mtcp_destroy_context(mctx);
	pthread_exit(NULL);

	return NULL;
}

static void *PollThread(void *arg)
{
	for (int i = 0; i < core_limit; i++) {
		pthread_join(app_thread[i], NULL);
	}

	mtcp_destroy();
	pthread_exit(NULL);

	return NULL;
}

static void SignalHandler(int signum)
{
	for (int i = 0; i < core_limit; i++) {
		if (app_thread[i] == pthread_self()) {
			done[i] = true;
		} else {
			if (!done[i]) {
				pthread_kill(app_thread[i], signum);
			}
		}
	}
}

int init_httpServer(int core, char *conf_file)
{
	core_limit = core;
	if (!conf_file) {
		printf("pass mTCP configuration file\n");
		return -1;
	}

	int ret = mtcp_init(conf_file);
	if (ret) {
		printf("mtcp_init failed\n");
		return -1;
	}

	mtcp_register_signal(SIGINT, SignalHandler);

	int cores[MAX_CPUS];
	for (int i = 0; i < core_limit; i++) {
		cores[i] = i;
		done[i] = false;

		ret = pthread_create(&app_thread[i], NULL, RunServerThread, &cores[i]);
		if (ret) {
			printf("pthread_create failed\n");
			return -1;
		}
	}

	pthread_create(&poll_thread, NULL, PollThread, NULL);

	return 0;
}
