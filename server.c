#include "common.h"

#include <stdatomic.h>

#include "encoder.h"
#include "httpServer.h"
#include "gpuManager.h"

const char *module_name = "inference";
const char *test_model = "resnet50";
const char *image_path = "data/example.jpg";

static pthread_t httpThread[MAX_CPUS];
static pthread_t gpuThread[MAX_CPUS];
static pthread_t rmThread[MAX_CPUS];

static struct sq *gsq[MAX_CPUS];
static struct cq *gcq[MAX_CPUS];

static atomic_int sq_cnt[MAX_CPUS];
static atomic_int cq_cnt[MAX_CPUS];

static int get_test_data(char *buf, int *size)
{
    FILE *fp = fopen(image_path, "rb");
    if (!fp) {
        printf("fopen failed: %s\n", __func__);
        return -1;
    }

    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    *size = fread(buf, sizeof(char), *size, fp);
	if (!*size) {
        printf("fread failed\n");
        return -1;
    }

    fclose(fp);

    return 0;
}

static void *gpu_worker(void *arg)
{
	InferenceContext *ctx;
	struct sq *sq;
	struct cq *cq;
	request_t *req;
	int core;
	int cnt;
	int i, ret;

	core = *(int *) arg;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (ret) {
		perror("pthread_setaffinity_np failed\n");
		return NULL;
	}

	ctx = initialize_inference(module_name);
	if (!ctx) {
		printf("initialize_inference failed\n");
		return NULL;
	}

	sq = gsq[core];
	cq = gcq[core];

	printf("gpu worker is working now ...\n");

	while (true) {
		cnt = atomic_load(&sq_cnt[core]);

		for (i = 0; i < cnt; i++) {
			req = TAILQ_FIRST(sq);
			if (!req) {
				continue;
			}
			TAILQ_REMOVE(sq, req, req_entries);

			request_t *res = calloc(1, sizeof(request_t));
			if (!res) {
				printf("calloc failed\n");
				return NULL;
			}
	
			if (!req->model) {
				memcpy(req->model, test_model, sizeof(*test_model));
			}

			ret = load_model(ctx, req->model);
			if (ret) {
				printf("load_model failed\n");
				goto failed;
			}

			if (!req->image || !strcmp(req->image, "")) {
				ret = get_test_data(req->image, &req->size);
				if (!req->image || ret) {
					printf("get_test_data failed\n");
					goto failed;
				}
			} else {
				if (base64_decode(req->image, sizeof(req->image), &req->size)) {
					printf("base64_decode failed\n");
					goto failed;
				}
			}

			ctx->input = preprocess_on_cpu(ctx, req->image, req->size);
			if (!ctx->input) {
				printf("preprocess_on_cpu failed\n");
				goto failed;
			}

			ctx->output = run_inference(ctx);
			if (!ctx->output) {
				printf("run_inference failed\n");
				goto failed;
			}

			const char *result = postprocess_on_cpu(ctx);
			if (!result) {
				printf("postprocess_on_cpu failed\n");
				goto failed;
			}

			//printf("result:\n%s\n", result);

			res->sock_id = req->sock_id;
			res->ep = req->ep;
			res->size = req->size;
			memcpy(req->result, (char *) result, strlen(result));
			TAILQ_INSERT_TAIL(cq, res, req_entries);

			atomic_fetch_add(&cq_cnt[core], 1);
			atomic_fetch_sub(&sq_cnt[core], 1);
			continue;

failed:
			res->sock_id = req->sock_id;
			res->ep = req->ep;
			res->size = req->size;
			memcpy(res->result, "failed", sizeof("failed"));
			TAILQ_INSERT_TAIL(cq, res, req_entries);
			
			atomic_fetch_add(&cq_cnt[core], 1);
			atomic_fetch_sub(&sq_cnt[core], 1);
			continue;
		}
	}

	pthread_exit(NULL);

	return NULL;
}

static void *rm_worker(void *arg)
{
	struct cq *cq;
	request_t *req;
	int core;
	int cnt;
	int i, ret;

	core = *(int *) arg;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (ret) {
		perror("pthread_setaffinity_np failed\n");
		return NULL;
	}

	cq = gcq[core];

	printf("CQ worker is working now...\n");

	while (true) {
		cnt = atomic_load(&cq_cnt[core]);

		for (i = 0; i < cnt; i++) {
			req = TAILQ_FIRST(cq);
			if (!req) {
				continue;
			}

			TAILQ_REMOVE(cq, req, req_entries);

			struct epoll_event event;
			event.events = EPOLLOUT;
			event.data.fd = req->sock_id;
			epoll_ctl(req->ep, EPOLL_CTL_MOD, req->sock_id, &event);

			atomic_fetch_sub(&cq_cnt[core], 1);
		}
	}

	pthread_exit(NULL);

	return NULL;
}

static void *http_worker(void *arg)
{
	struct sq *sq;
	struct epoll_event events[MAX_EVENTS];
	struct flow_context *ctx;
	int ep;
	int listener;
	int nevents;
	int do_accept;
	int core;
	int i, ret;

	core = *(int *) arg;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (ret) {
		perror("pthread_setaffinity_np failed\n");
		return NULL;
	}

	sq = gsq[core];

	ctx = init_server(core);
	if (!ctx) {
		printf("init_server failed\n");
		return NULL;
	}
	ep = ctx->ep;

	listener = create_socket(ep);
	if (listener < 0) {
		printf("create_socket failed\n");
		return NULL;
	}

	sq = gsq[core];

	printf("Server is listening on port %d...\n", PORT);

	while (true) {
		nevents = epoll_wait(ep, events, MAX_EVENTS, -1);
		if (nevents < 0) {
			if (errno != EINTR) {
				perror("epoll_wait failed");
			}

			break;
		}

		do_accept = false;
		for (i = 0; i < nevents; i++) {
			if (events[i].data.fd == listener) {
				do_accept = true;
			} else if (events[i].events & EPOLLERR) {
				printf("[CPU %d]: Error on socket %d\n", 
						core, events[i].data.fd);
				close_connection(ep, events[i].data.fd);
			} else if (events[i].events & EPOLLIN) {
				request_t *req = (request_t *) malloc(sizeof(request_t));
				if (!req) {
					perror("malloc failed");
					return NULL;
				}

				ret = handle_read(events[i].data.fd, ctx,
						&ctx->svars[events[i].data.fd], req);
				if (ret) {
					if (errno != EAGAIN) {
						close_connection(ep, events[i].data.fd);
					}

					printf("[CPU %d]: Error on socket %d\n",
							core, events[i].data.fd);
				}

				TAILQ_INSERT_TAIL(sq, req, req_entries);
				atomic_fetch_add(&sq_cnt[core], 1);
			} else if (events[i].events & EPOLLOUT) {
				ret = handle_write(events[i].data.fd, ctx,
						&ctx->svars[events[i].data.fd]);
				if (ret) {
					printf("[CPU %d]: Error on socket %d\n",
							core, events[i].data.fd);
				}
			} else {
				assert(0);
			}
		}

		if (do_accept) {
			while (1) {
				ret = handle_accept(listener, ctx);
				if (ret < 0) {
					break;
				}
			}
		}
	}

	epoll_ctl(ep, EPOLL_CTL_DEL, listener, NULL);
	free(ctx);
	pthread_exit(NULL);

	return NULL;
}

static void usage(void)
{
	printf("[Usage]: ");
	printf("./server [-c <process core>] [-n <nr core>]\n");
	exit(1);
}

static void init_request_manager(int i)
{
	gsq[i] = malloc(sizeof(struct sq));
	if (!gsq[i]) {
		printf("malloc failed\n");
		return;
	}

	gcq[i] = malloc(sizeof(struct cq));
	if (!gcq[i]) {
		printf("malloc failed\n");
		return;
	}

	TAILQ_INIT(gsq[i]);
	TAILQ_INIT(gcq[i]);
}

int main(int argc, char *argv[])
{
	int option;
	int core_limit = 0;
	int cores[MAX_CPUS];
	int ret;

	while ((option = getopt(argc, argv, "n:")) != -1) {
		switch (option) {
			case 'n':
				core_limit = strtol(optarg, NULL, 0);
				break;
			default:
				usage();
		}
	}

	core_limit = (core_limit == 0) ? 1 : core_limit;

	for (int i = 0; i < core_limit; i++) {
		cores[i] = i;

		init_request_manager(i);

		ret = pthread_create(&httpThread[i], NULL, http_worker, &cores[i]);
		if (ret) {
			perror("pthread_create failed\n");
			return -1;
		}

		ret = pthread_create(&gpuThread[i], NULL, gpu_worker, &cores[i]);
		if (ret) {
			perror("pthread_create failed\n");
			return -1;
		}

		ret = pthread_create(&rmThread[i], NULL, rm_worker, &cores[i]);
		if (ret) {
			perror("pthread_create failed\n");
			return -1;
		}
	}

	pthread_join(httpThread[0], NULL);

    return 0;
}
