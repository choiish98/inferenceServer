#ifndef GPU_H
#define GPU_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <Python.h>
#include <cuda.h>
#include <cuda_runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	PyObject *module;
	PyObject *model;
} InferenceContext;

typedef struct {
	PyObject *tensor;
	void *gpu_data;
	size_t size;
} InputTensor;

void *init_gpu(size_t, const char *);
int free_gpu(void *);

void gpuMemAlloc(void **, int);
void gpuMemFree(float *);

void gpuPinnedMemAlloc(void **, int);
void gpuPinnedMemMap(void **, void **);
void gpuPinnedMemFree(float *);

int inferCudaMemcpy(float *, float *, int);
int inferZeroCopy(float *, int);

InferenceContext *initialize_inference(const char *);
int load_model(InferenceContext *);
void free_tensor(InputTensor *);
void free_inference_context(InferenceContext *);

InputTensor *allocate_gpu_memory(size_t);
int copy_to_gpu(InputTensor *, PyObject *);

PyObject *preprocess_on_cpu(InferenceContext *, const void *, size_t);
PyObject *postprocess_on_cpu(InferenceContext *, PyObject *);

InputTensor *run_inference(InferenceContext *, InputTensor *);

#ifdef __cplusplus
}
#endif

#endif
