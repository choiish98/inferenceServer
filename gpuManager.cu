extern "C" {
	#include "gpuManager.h"
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
int load_model(InferenceContext *ctx, const char *model) {
    PyObject *pFuncLoadModel = 
		PyObject_GetAttrString(ctx->module, "load_model");
    if (!pFuncLoadModel || !PyCallable_Check(pFuncLoadModel)) {
        PyErr_Print();
        return -1;
    }

	PyObject *pArgsLoadModel = PyTuple_Pack(1, PyUnicode_FromString(model));
    ctx->model = PyObject_CallObject(pFuncLoadModel, NULL);
    Py_DECREF(pArgsLoadModel);
    Py_DECREF(pFuncLoadModel);

    if (!ctx->model) {
        PyErr_Print();
        return -1;
    }

    return 0;
}

// Preprocess data on CPU
PyObject *preprocess_on_cpu(InferenceContext *ctx, char *image) {
    PyObject *pFuncPreprocess = 
		PyObject_GetAttrString(ctx->module, "preprocess");
    if (!pFuncPreprocess || !PyCallable_Check(pFuncPreprocess)) {
        PyErr_Print();
        return NULL;
    }

    PyObject *pArgsPreprocess = PyTuple_Pack(1, PyUnicode_FromString(image));
    PyObject *pInput = PyObject_CallObject(pFuncPreprocess, pArgsPreprocess);
    Py_DECREF(pArgsPreprocess);
    Py_DECREF(pFuncPreprocess);

    if (!pInput) {
        PyErr_Print();
        return NULL;
    }

    return pInput;
}

// Perform inference and keep the result in GPU memory
PyObject *run_inference(InferenceContext *ctx) {
    PyObject *pFuncInfer = PyObject_GetAttrString(ctx->module, "infer");
    if (!pFuncInfer || !PyCallable_Check(pFuncInfer)) {
        PyErr_Print();
        return NULL;
    }

    PyObject *pArgsInfer = PyTuple_Pack(2, ctx->model, ctx->input);
    PyObject *pOutput = PyObject_CallObject(pFuncInfer, pArgsInfer);
    Py_DECREF(pArgsInfer);
    Py_DECREF(pFuncInfer);

    if (!pOutput) {
        PyErr_Print();
        return NULL;
    }

    return pOutput;
}

// Postprocess the inference result on CPU
PyObject *postprocess_on_cpu(InferenceContext *ctx) {
    PyObject *pFuncPostprocess = 
		PyObject_GetAttrString(ctx->module, "postprocess");
    if (!pFuncPostprocess || !PyCallable_Check(pFuncPostprocess)) {
        PyErr_Print();
        return NULL;
    }

    PyObject *pArgsPostprocess = PyTuple_Pack(1, ctx->output);
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
void free_inference(InferenceContext *ctx) {
    if (ctx) {
		Py_DECREF(ctx->result);
		Py_DECREF(ctx->output);
		Py_DECREF(ctx->input);
        Py_DECREF(ctx->model);
        Py_DECREF(ctx->module);
        free(ctx);
    }

    Py_Finalize();
}

