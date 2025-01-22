#include "gpu_mem.h"

static CUcontext cuContext;

static void checkCuError(CUresult err, const char *msg)
{
	if (err != CUDA_SUCCESS) {
		printf("Cuda Error: %s\n", msg);
		exit(EXIT_FAILURE);
	}
}

static void checkCudaError(cudaError_t err, const char *msg)
{
	if (err != cudaSuccess) {
		printf("%s: %s\n", msg, cudaGetErrorString(err));
		exit(EXIT_FAILURE);
	}
}

static int get_gpu_device_id(const char *bdf)
{
	return 0;
}

void *init_gpu(size_t size, const char *bdf)
{
    const size_t gpu_page_size = 64 * 1024;
    CUdevice cu_dev;
    CUdeviceptr d_A;

    size_t aligned_size = (size + gpu_page_size - 1) & ~(gpu_page_size - 1);

    int dev_id = get_gpu_device_id(bdf);
    if (dev_id < 0) {
        printf("Wrong device index (%d) obtained from bdf \"%s\"\n",
                dev_id, bdf);
        return NULL;
    }

    checkCuError(cuInit(0), "cuInit failed");
    checkCuError(cuDeviceGet(&cu_dev, dev_id), "cuDeviceGet failed");
    checkCuError(cuCtxCreate(&cuContext, CU_CTX_MAP_HOST, cu_dev), 
			"cuCtxCreate failed");
    checkCuError(cuMemAlloc(&d_A, aligned_size), "cuMemAlloc failed");

    return ((void *) d_A);
}

int free_gpu(void *gpu_buf)
{
    CUdeviceptr d_A = (CUdeviceptr) gpu_buf;

    cuMemFree(d_A);
    d_A = 0;

    checkCuError(cuCtxDestroy(cuContext), "cuCtxDestroy failed");

    return 0;
}

void gpuMemAlloc(void **buf, int size)
{
	checkCudaError(cudaMalloc(buf, size), "cudaMalloc failed");
}

void gpuMemFree(float *buf)
{
	cudaFree(buf);
}

void gpuPinnedMemAlloc(void **buf, int size)
{
	checkCudaError(cudaHostAlloc(buf, size, cudaHostAllocMapped),
			"cudaHostAlloc failed");
}

void gpuPinnedMemMap(void **gpu, void **host)
{
	checkCudaError(cudaHostGetDevicePointer(gpu, host, 0),
			"cudaHostGetDevicePointer failed");
}

void gpuPinnedMemFree(float *buf)
{
	cudaFreeHost(buf);
}

/* Simple GPU kernel */
static __global__ void squareKernel(float *data, int size)
{
	int idx = blockIdx.x * blockDim.x + threadIdx.x;
	if (idx < size) {
		data[idx] *= data[idx];
	}
}

int inferCudaMemcpy(float *host, float *gpu, int size)
{
	int threadsPerBlock = 256;
	int blocksPerGrid = (size + threadsPerBlock - 1) / threadsPerBlock;

	/* Host -> GPU memory copy */
	checkCudaError(cudaMemcpy(gpu, host, size, cudaMemcpyHostToDevice),
			"cudaMemcpy failed");

	/* Inference */
	squareKernel<<<blocksPerGrid, threadsPerBlock>>>(gpu, size);
	cudaError_t err = cudaDeviceSynchronize();
	if (err != cudaSuccess) {
		printf("Cuda Error: %s\n", cudaGetErrorString(err));
	}

	/* GPU -> Host memory copy */
	checkCudaError(cudaMemcpy(host, gpu, size, cudaMemcpyDeviceToHost),
			"cudaMemcpy failed");

	return 0;
}

int inferZeroCopy(float *gpu, int size)
{
	int threadsPerBlock = 256;
	int blocksPerGrid = (size + threadsPerBlock - 1) / threadsPerBlock;

	/* Inference */
	squareKernel<<<blocksPerGrid, threadsPerBlock>>>(gpu, size);
	cudaError_t err = cudaDeviceSynchronize();
	if (err != cudaSuccess) {
		printf("Cuda Error: %s\n", cudaGetErrorString(err));
	}

	return 0;
}

