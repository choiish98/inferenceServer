#include "common.h"

#include "httpServer.h"
#include "gpuManager.h"

const char *module_name = "inference";
const char *model_name = "resnet50";

static pthread_t reqThread;
static pthread_t resThread;

// inference example
static int infer(char *model_name, char *image_data, char *rst)
{
	printf("%s: start\n", __func__);

	// load python module
	InferenceContext *ctx = initialize_inference(module_name);
	if (!ctx) {
		printf("initialize_inference failed\n");
		exit(EXIT_FAILURE);
	}

	// load model
	int ret = load_model(ctx, model_name);
	if (ret) {
		printf("load_model failed\n");
		goto free;
	}

	// load image file & preprocessing
	ctx->input = preprocess_on_cpu(ctx, "data/example.jpg");
	if (!ctx->input) {
		printf("preprocess_on_cpu failed\n");
		goto free;
	}

	// Inference
	ctx->output = run_inference(ctx);
	if (!ctx->output) {
		printf("run_inference failed\n");
		goto free;
	}

	// postprocessing
	ctx->result = postprocess_on_cpu(ctx);
	if (!ctx->result) {
		printf("postprocess_results failed\n");
		goto free;
	}

	/* Print the postprocessing result
	printf("Postprocessing result:\n");
	PyObject_Print(ctx->result, stdout, 0);
	printf("\n");	
	*/

	memcpy(rst, (const char *) ctx->result, sizeof(ctx->result));

	return 0;
free:
	free_inference(ctx);
	return -1;
}

static void *cq_worker(void *arg)
{
	request_manager rm = *(request_manager *) arg;
	int ret;

	// set core affinity
	int core = rm.cpu;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (ret) {
		perror("pthread_setaffinity_np failed\n");
		return NULL;
	}

	// polling CQ
	response_t *res;
	while (true) {
		res = TAILQ_FIRST(&rm.cq);
		if (!res) {
			continue;
		}

		TAILQ_REMOVE(&rm.cq, res, res_entries);

		// copy to rsp buffer
		//memcpy(&ctx->svars[core]->result, res->result, RESULT_SIZE);

		// epoll event
		struct epoll_event event;
		event.events = EPOLLOUT;
		event.data.fd = res->sock_id;
		epoll_ctl(res->ep, EPOLL_CTL_MOD, res->sock_id, &event);
	}

	pthread_exit(NULL);

	return NULL;
}

static void *sq_worker(void *arg)
{
	int ret;
	request_manager rm = *(request_manager *) arg;

	// set core affinity
	int core = rm.cpu;
	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core, &cpuset);

	ret = pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset);
	if (ret) {
		perror("pthread_setaffinity_np failed\n");
		return NULL;
	}

	// polling SQ
	request_t *req;
	while (true) {
		req = TAILQ_FIRST(&rm.sq);
		if (!req) {
			continue;
		}

		TAILQ_REMOVE(&rm.sq, req, req_entries);

		// inference
		ret = infer(req->model, req->image, req->result);
		if (ret) {
			printf("infer failed\n");
			return NULL;
		}

		// response
		response_t *res = calloc(1, sizeof(response_t));
		res->sock_id = req->sock_id;
		res->ep = req->ep;
		memcpy(res->result, req->result, RESULT_SIZE);
		TAILQ_INSERT_TAIL(&rm.cq, res, res_entries);
	}

	pthread_exit(NULL);

	return NULL;
}

static void usage(void)
{
	printf("[Usage]: ");
	printf("./server [-c <process core>] [-n <nr core>]\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int option;
	int proc = -1;
	int core_limit = 0;

	while ((option = getopt(argc, argv, "c:n:")) != -1) {
		switch (option) {
			case 'c':
				proc = strtol(optarg, NULL, 0);
				break;
			case 'n':
				core_limit = strtol(optarg, NULL, 0);
				break;
			default:
				usage();
		}
	}

	proc = (proc == -1) ? 0 : proc;
	core_limit = (core_limit == 0) ? 1 : core_limit;

	// Request Manager init
	request_manager rm[MAX_CPUS];
	struct request_head sq[MAX_CPUS];
	struct response_head cq[MAX_CPUS];
	for (int i = proc; i < proc + core_limit; i++) {
		TAILQ_INIT(&sq[i]);
		TAILQ_INIT(&cq[i]);

		rm[i].cpu = i;
		rm[i].sq = sq[i];
		rm[i].cq = cq[i];

		if (pthread_create(&reqThread, NULL, sq_worker, &rm[i]) != 0) {
			perror("pthread_create failed");
			return -1;
		}

		if (pthread_create(&resThread, NULL, cq_worker, &rm[i]) != 0) {
			perror("pthread_create failed");
			return -1;
		}
	}

	// HTTP server init
	int ret = init_httpServer(proc, core_limit, rm);
	if (ret) {
		printf("init_httpServer failed\n");
		return -1;
	}

	poll_httpServer(proc, core_limit);

    return 0;
}
