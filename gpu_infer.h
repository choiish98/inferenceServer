#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>
#include <string.h>

#include <Python.h>

// Opaque structures for Python objects
typedef struct {
	PyObject *module;
	PyObject *model;
} InferenceContext;

typedef struct {
	PyObject *tensor;
} InputTensor;

InferenceContext *initialize_inference(const char *);
int load_model(InferenceContext *);
InputTensor *preprocess_image(InferenceContext *, const char *);
InputTensor *run_inference(InferenceContext *, InputTensor *);
int postprocess_results(InferenceContext *, InputTensor *, int);
void free_tensor(InputTensor *);
void free_inference_context(InferenceContext *);
