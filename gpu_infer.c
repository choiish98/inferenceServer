#include "gpu_infer.h"

// Initialize the Python interpreter and load the module
InferenceContext *initialize_inference(const char *module_name) {
    Py_Initialize();

    PyObject *sysPath = PySys_GetObject("path");
    PyObject *currentDir = PyUnicode_FromString(".");
    PyList_Append(sysPath, currentDir);
    Py_DECREF(currentDir);

    PyObject *pName = PyUnicode_FromString(module_name);
    PyObject *pModule = PyImport_Import(pName);
    if (!pModule) {
        PyErr_Print();
        Py_Finalize();
        return NULL;
    }

    Py_DECREF(pName);

    InferenceContext *ctx = malloc(sizeof(InferenceContext));
    ctx->module = pModule;
    ctx->model = NULL;

    return ctx;
}

// Load the model
int load_model(InferenceContext *ctx) {
    PyObject *pFuncLoadModel;

    pFuncLoadModel = PyObject_GetAttrString(ctx->module, "load_model");
    if (!pFuncLoadModel || !PyCallable_Check(pFuncLoadModel)) {
        PyErr_Print();
        return -1;
    }

    ctx->model = PyObject_CallObject(pFuncLoadModel, NULL);
    if (!ctx->model) {
        PyErr_Print();
        return -1;
    }

    Py_DECREF(pFuncLoadModel);

    return 0;
}

// Preprocess the image
InputTensor *preprocess_image(InferenceContext *ctx, const char *image_path) {
    PyObject *pFuncPreprocess;
    PyObject *pArgsPreprocess;
    PyObject *pInputTensor;

    pFuncPreprocess = PyObject_GetAttrString(ctx->module, "preprocess");
    if (!pFuncPreprocess || !PyCallable_Check(pFuncPreprocess)) {
        PyErr_Print();
        return NULL;
    }

    pArgsPreprocess = PyTuple_Pack(1, PyUnicode_FromString(image_path));
    pInputTensor = PyObject_CallObject(pFuncPreprocess, pArgsPreprocess);
    if (!pInputTensor) {
        PyErr_Print();
        return NULL;
    }

    Py_DECREF(pArgsPreprocess);
    Py_DECREF(pFuncPreprocess);

    InputTensor *input = malloc(sizeof(InputTensor));
    input->tensor = pInputTensor;

    return input;
}

// Perform inference
InputTensor *run_inference(InferenceContext *ctx, InputTensor *input) {
    PyObject *pFuncInfer;
    PyObject *pArgsInfer;
    PyObject *pOutput;

    pFuncInfer = PyObject_GetAttrString(ctx->module, "infer");
    if (!pFuncInfer || !PyCallable_Check(pFuncInfer)) {
        PyErr_Print();
        return NULL;
    }

    pArgsInfer = PyTuple_Pack(2, ctx->model, input->tensor);
    pOutput = PyObject_CallObject(pFuncInfer, pArgsInfer);
    if (!pOutput) {
        PyErr_Print();
        return NULL;
    }

    Py_DECREF(pArgsInfer);
    Py_DECREF(pFuncInfer);

    InputTensor *output = malloc(sizeof(InputTensor));
    output->tensor = pOutput;

    return output;
}

// Postprocess the inference output
int postprocess_results(InferenceContext *ctx, InputTensor *output, int top_k) {
    PyObject *pFuncPostprocess;
    PyObject *pArgsPostprocess;
    PyObject *pResults;

    pFuncPostprocess = PyObject_GetAttrString(ctx->module, "postprocess");
    if (!pFuncPostprocess || !PyCallable_Check(pFuncPostprocess)) {
        PyErr_Print();
        return -1;
    }

    pArgsPostprocess = PyTuple_Pack(2, output->tensor, PyLong_FromLong(top_k));
    pResults = PyObject_CallObject(pFuncPostprocess, pArgsPostprocess);
    if (!pResults) {
        PyErr_Print();
        return -1;
    }

    Py_DECREF(pArgsPostprocess);
    Py_DECREF(pFuncPostprocess);

    // Print the results
    PyObject *pIter = PyObject_GetIter(pResults);
    PyObject *pItem;

    printf("Top-%d Results:\n", top_k);
    while ((pItem = PyIter_Next(pIter))) {
        PyObject *pLabel = PyTuple_GetItem(pItem, 0);
        PyObject *pProb = PyTuple_GetItem(pItem, 1);

        const char *label = PyUnicode_AsUTF8(pLabel);
        double prob = PyFloat_AsDouble(pProb);

        printf("%s: %.4f\n", label, prob);
        Py_DECREF(pItem);
    }

    Py_DECREF(pIter);
    Py_DECREF(pResults);

    return 0;
}

// Free resources
void free_tensor(InputTensor *tensor) {
    if (tensor) {
        Py_DECREF(tensor->tensor);
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

