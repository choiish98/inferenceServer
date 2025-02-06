#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <malloc.h>
#include <time.h>

//#include "mtcp.h"
//#include "tcp.h"
#include "gpu.h"

const char *module_name = "inference";
const char *model_name = "resnet50";

static void measure(void)
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

	// Print the postprocessing result
	printf("Postprocessing result:\n");
	PyObject_Print(ctx->result, stdout, 0);
	printf("\n");	

free:
	free_inference(ctx);
}

int main(void)
{
	/* HTTP server init
	int ret = init_httpTCPServer(core_limit, conf_file);
	if (ret) {
		printf("init_httpServer failed\n");
		return -1;
	}
	*/
	
	// Request Manager init
	// ??

	/* RDMA init 
	saddr.sin_family = AF_INET;

	printf("%s:%d\n", __func__, __LINE__);
	pthread_create(&server_thread, NULL, process_server, &size);
	printf("%s:%d\n", __func__, __LINE__);
	pthread_create(&client_thread, NULL, process_client, &size);
	printf("%s:%d\n", __func__, __LINE__);
	while (!rdma_is_connected());
	printf("%s:%d\n", __func__, __LINE__);

	printf("Successfully connected\n");
	*/

	/* Register GPU memory to RDMA memory region 
	void *gpu_buf = init_gpu(size, "bdf");
	if (!gpu_buf) {
		printf("init_gpu failed\n");
		return -1;
	}
	*/

	/*
	int ret = rdma_register_mr(gpu_buf, size, true);
	if (ret) {
		printf("rdma_regiser_mr failed\n");
		return -1;
	}
	*/

	measure();

    return 0;
}             
