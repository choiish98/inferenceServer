extern "C" {
	#include "gpu.h"
}

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

// Initialize the Python interpreter and load the module
InferenceContext *initialize_inference(const char *module_name) {
    Py_Initialize();

    PyObject *sysPath = PySys_GetObject("path");
    PyObject *currentDir = PyUnicode_FromString(".");
    PyList_Append(sysPath, currentDir);
    Py_DECREF(currentDir);

    PyObject *pName = PyUnicode_FromString(module_name);
    PyObject *pModule = PyImport_Import(pName);
    Py_DECREF(pName);

    if (!pModule) {
        PyErr_Print();
        Py_Finalize();
        return NULL;
    }

    InferenceContext *ctx = (InferenceContext *) 
		malloc(sizeof(InferenceContext));
    ctx->module = pModule;
    ctx->model = NULL;

    return ctx;
}

// Load the model
int load_model(InferenceContext *ctx) {
    PyObject *pFuncLoadModel = 
		PyObject_GetAttrString(ctx->module, "load_model");
    if (!pFuncLoadModel || !PyCallable_Check(pFuncLoadModel)) {
        PyErr_Print();
        return -1;
    }

    ctx->model = PyObject_CallObject(pFuncLoadModel, NULL);
    Py_DECREF(pFuncLoadModel);

    if (!ctx->model) {
        PyErr_Print();
        return -1;
    }

    return 0;
}

// Allocate GPU memory for input and output tensor
InputTensor *allocate_gpu_memory(size_t size) {
    InputTensor *input = (InputTensor *) malloc(sizeof(InputTensor));
    if (!input) {
        fprintf(stderr, "Failed to allocate InputTensor structure\n");
        return NULL;
    }

    if (cudaMalloc(&input->gpu_data, size) != cudaSuccess) {
        fprintf(stderr, "Failed to allocate GPU memory\n");
        free(input);
        return NULL;
    }
    input->size = size;
    input->tensor = NULL;

    return input;
}

// Preprocess data on CPU
PyObject *preprocess_on_cpu(InferenceContext *ctx, 
		const void *cpu_data, size_t size) {
    PyObject *pFuncPreprocess = 
		PyObject_GetAttrString(ctx->module, "preprocess");
    if (!pFuncPreprocess || !PyCallable_Check(pFuncPreprocess)) {
        PyErr_Print();
        return NULL;
    }

    PyObject *pCpuData = 
		PyMemoryView_FromMemory((char *) cpu_data, size, PyBUF_READ);
	size_t num_floats = size / sizeof(float);
	PyObject *pSize = PyLong_FromSize_t(num_floats);
    PyObject *pArgsPreprocess = PyTuple_Pack(2, pCpuData, pSize);

    PyObject *pProcessedTensor = 
		PyObject_CallObject(pFuncPreprocess, pArgsPreprocess);
    Py_DECREF(pCpuData);
    Py_DECREF(pArgsPreprocess);
    Py_DECREF(pFuncPreprocess);

    if (!pProcessedTensor) {
        PyErr_Print();
        return NULL;
    }

    return pProcessedTensor;
}

// Copy CPU processed data to GPU
int copy_to_gpu(InputTensor *input, PyObject *pProcessedTensor) {
    void *cpu_memory = PyMemoryView_GET_BUFFER(pProcessedTensor);
    if (cudaMemcpy(input->gpu_data, cpu_memory, input->size, 
				cudaMemcpyHostToDevice) != cudaSuccess) {
        fprintf(stderr, "Failed to copy preprocessed data to GPU memory\n");
        Py_DECREF(pProcessedTensor);
        return -1;
    }

    input->tensor = pProcessedTensor;

    return 0;
}

// Perform inference and keep the result in GPU memory
InputTensor *run_inference(InferenceContext *ctx, InputTensor *input) {
    PyObject *pFuncInfer = PyObject_GetAttrString(ctx->module, "infer");
    if (!pFuncInfer || !PyCallable_Check(pFuncInfer)) {
        PyErr_Print();
        return NULL;
    }

    PyObject *pArgsInfer = PyTuple_Pack(2, ctx->model, input->tensor);
    PyObject *pOutputGpu = PyObject_CallObject(pFuncInfer, pArgsInfer);
    Py_DECREF(pArgsInfer);
    Py_DECREF(pFuncInfer);

    if (!pOutputGpu) {
        PyErr_Print();
        return NULL;
    }

    InputTensor *output = (InputTensor *) malloc(sizeof(InputTensor));
    output->tensor = pOutputGpu;
    output->gpu_data = NULL;
    output->size = 0;

    return output;
}

// Postprocess the inference result on CPU
PyObject *postprocess_on_cpu(InferenceContext *ctx, PyObject *output_tensor) {
    PyObject *pFuncPostprocess = 
		PyObject_GetAttrString(ctx->module, "postprocess");
    if (!pFuncPostprocess || !PyCallable_Check(pFuncPostprocess)) {
        PyErr_Print();
        return NULL;
    }

    PyObject *pArgsPostprocess = PyTuple_Pack(1, output_tensor);
    PyObject *pProcessedResult = PyObject_CallObject(pFuncPostprocess, 
			pArgsPostprocess);
    Py_DECREF(pArgsPostprocess);
    Py_DECREF(pFuncPostprocess);

    if (!pProcessedResult) {
        PyErr_Print();
        return NULL;
    }

    return pProcessedResult;
}

// Free resources
void free_tensor(InputTensor *tensor) {
    if (tensor) {
        if (tensor->gpu_data) cudaFree(tensor->gpu_data);
        if (tensor->tensor) Py_DECREF(tensor->tensor);
        free(tensor);
    }
}

void free_inference_context(InferenceContext *ctx) {
    if (ctx) {
        if (ctx->model) Py_DECREF(ctx->model);
        Py_DECREF(ctx->module);
        free(ctx);
    }

    Py_Finalize();
}

