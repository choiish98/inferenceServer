#ifndef GPU_H
#define GPU_H

#include <stdio.h>

#include <cuda.h>
#include <cuda_runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

void *init_gpu(size_t, const char *);
int free_gpu(void *);

void gpuMemAlloc(void **, int);
void gpuMemFree(float *);

void gpuPinnedMemAlloc(void **, int);
void gpuPinnedMemMap(void **, void **);
void gpuPinnedMemFree(float *);

int inferCudaMemcpy(float *, float *, int);
int inferZeroCopy(float *, int);

#ifdef __cplusplus
}
#endif

#endif
