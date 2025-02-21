#ifndef GPUMANAGER_H
#define GPUMANAGER_H

#include "common.h"

#include <Python.h>
#include <cuda.h>
#include <cuda_runtime.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	PyObject *module;
	PyObject *model;
	PyObject *input;
	PyObject *output;
	PyObject *result;
} InferenceContext;

InferenceContext *initialize_inference(const char *);
void free_inference(InferenceContext *);

int load_model(InferenceContext *, const char *);

PyObject *preprocess_on_cpu(InferenceContext *, char *);
PyObject *postprocess_on_cpu(InferenceContext *);

PyObject *run_inference(InferenceContext *);

#ifdef __cplusplus
}
#endif

#endif
