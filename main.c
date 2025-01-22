#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <malloc.h>
#include <time.h>

#include "rdma.h"
#include "gpu_mem.h"
#include "gpu_infer.h"

struct sockaddr_in saddr;

/*
pthread_t server_thread;
pthread_t client_thread;

static void *process_server(void *arg)
{
	int size = *(int *) arg;

	printf("%s:%d\n", __func__, __LINE__);
	int ret = rdma_open_server(&saddr, size);
	if (ret) {
		printf("rdma_open_server failed\n");
	}

	printf("%s:%d\n", __func__, __LINE__);
	pthread_exit(NULL);
}

static void *process_client(void *arg)
{
	int size = *(int *) arg;

	printf("%s:%d\n", __func__, __LINE__);
	int ret = rdma_open_client(&saddr, &saddr, size);
	if (ret) {
		printf("rdma_open_client failed\n");
	}

	printf("%s:%d\n", __func__, __LINE__);
	pthread_exit(NULL);
}
*/

static void measureCudaMemcpy(int size)
{
	const int iter = 100;

	printf("%s: start\n", __func__);

	/* Memory allocation */
	float *hostMemory = (float *) calloc(1, size);
    if (!hostMemory) {
        printf("calloc failed\n");
        exit(EXIT_FAILURE);
    }

	float *gpuMemory;
	gpuMemAlloc((void *) &gpuMemory, size);
	if (!gpuMemory) {
		printf("gpuMemAlloc failed\n");
		free(hostMemory);
		exit(EXIT_FAILURE);
	}

	/* Data load & preprocessing */ 
	for (int i = 0; i < size/sizeof(float); i++) {
		hostMemory[i] = (float) i;
	}

	/* Inference */
	clock_t start, end;
	double total = 0;

	for (int i = 0; i < iter; i++) {
		start = clock();
		inferCudaMemcpy(hostMemory, gpuMemory, size);
		end = clock();

		total += (double) (end - start) / CLOCKS_PER_SEC;
	}

	printf("[cudaMemcpy] Size: %d Bytes\n Avg Latency: %.6f seconds\n",
			size, total / iter);

	gpuMemFree(gpuMemory);
	free(hostMemory);
}

static void measureGPUZeroCopy(int size)
{
	const int iter = 100;

	printf("%s: start\n", __func__);

	/* Memory allocation */
	float *hostMemory;
	gpuPinnedMemAlloc((void **) &hostMemory, size);
	if (!hostMemory) {
		printf("gpuPinnedMemAlloc failed\n");
		exit(1);
	}

	float *gpuMemory; 
	gpuPinnedMemMap((void **) &hostMemory, (void **) &gpuMemory);
	if (!gpuMemory) {
		printf("gpuPinnedMemMap failed\n");
		free(hostMemory);
		exit(EXIT_FAILURE);
	}

	/* Data load & preprocessing */ 
	for (int i = 0; i < size; i++) {
		hostMemory[i] = (int) i;
	}

	/* Inference */
	clock_t start, end;
	double total = 0;

	for (int i = 0; i < iter; i++) {
		start = clock();
		inferZeroCopy(gpuMemory, size);
		end = clock();

		total += (double) (end - start) / CLOCKS_PER_SEC;
	}

	printf("[GPU Zero Copy] Size: %d Bytes, Avg Latency: %.6f seconds\n",
			size, total / iter);

	gpuPinnedMemFree(hostMemory);
}

static void measureGPUDirect(int size)
{
//	const int iter = 100;

	printf("%s: start\n", __func__);


}

static void get_addr(char *dst, struct sockaddr_in *addr)
{
	struct addrinfo *res;

	int ret = getaddrinfo(dst, NULL, NULL, &res);
	if (ret) {
		printf("getaddrinfo failed\n");
		exit(1);
	}

	if (res->ai_family == PF_INET) {
		memcpy(addr, res->ai_addr, sizeof(struct sockaddr_in));
	} else {
		exit(1);
	}

	freeaddrinfo(res);
}

static void usage(void)
{
	printf("[Usage]: ");
	printf("./main [-m <method>] [-c <client ip>] [-p <port>] [-s <data size>]\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	int option;
	int method = 0;
	int size = 0;

	while ((option = getopt(argc, argv, "m:c:p:s:")) != -1) {
		switch (option) {
			case 'm':
				method = strtol(optarg, NULL, 0);
				break;
			case 'c':
				get_addr(optarg, &saddr);
				break;
			case 'p':
				saddr.sin_port = htons(strtol(optarg, NULL, 0));
				break;
			case 's':
				size = strtol(optarg, NULL, 0);
				break;
			default:
				usage();
		}
	}

	if (method > 1) {
		goto test;
	}

	int ret = infer();
	if (ret) {
		printf("infer failed\n");
		return -1;
	}

	/* RDMA init 
	saddr.sin_family = AF_INET;

	printf("%s:%d\n", __func__, __LINE__);
	pthread_create(&server_thread, NULL, process_server, &size);
	printf("%s:%d\n", __func__, __LINE__);
	pthread_create(&client_thread, NULL, process_client, &size);
	printf("%s:%d\n", __func__, __LINE__);
	while (!rdma_is_connected());
	printf("%s:%d\n", __func__, __LINE__);
	*/

	printf("Successfully connected\n");

	/* Register GPU memory to RDMA memory region */
	void *gpu_buf = init_gpu(size, "bdf");
	if (!gpu_buf) {
		printf("init_gpu failed\n");
		return -1;
	}

	/*
	int ret = rdma_register_mr(gpu_buf, size, true);
	if (ret) {
		printf("rdma_regiser_mr failed\n");
		return -1;
	}
	*/

test:
	if (method == 0) {
		measureCudaMemcpy(size);
	} else if (method == 1) {
		measureGPUZeroCopy(size);
	} else if (method == 2) {
		measureGPUDirect(size);
	} else if (method == 3) {
		measureCudaMemcpy(size);
		measureGPUZeroCopy(size);
		measureGPUDirect(size);
	} else {
		usage();
	}

    return 0;
}             
