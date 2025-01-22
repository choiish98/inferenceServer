#include "gpu_infer.h"

int infer(void)
{
    // Initialize Python interpreter
    Py_Initialize();

	// Add this snippet before importing the module
	PyObject *sysPath = PySys_GetObject("path");
	PyObject *currentDir = PyUnicode_FromString(".");
	PyList_Append(sysPath, currentDir);
	Py_DECREF(currentDir);

    // Import Python script (inference.py)
    PyObject *pName = PyUnicode_FromString("inference");
    PyObject *pModule = PyImport_Import(pName);
    Py_DECREF(pName);
    if (!pModule) {
        PyErr_Print();
        Py_Finalize();
        return -1;
    }

	// Call the preprocess function to create a tensor
    PyObject *pFuncPreprocess = PyObject_GetAttrString(pModule, "preprocess");
    if (!pFuncPreprocess || !PyCallable_Check(pFuncPreprocess)) {
        PyErr_Print();
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    // Pass the image path to the preprocess function
    PyObject *pArgsPreprocess = PyTuple_Pack(1, PyUnicode_FromString("example.jpg")); 
    PyObject *pInputTensor = PyObject_CallObject(pFuncPreprocess, pArgsPreprocess);
    Py_DECREF(pArgsPreprocess);
    Py_DECREF(pFuncPreprocess);
    if (!pInputTensor) {
        PyErr_Print();
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    // Load the model by calling load_model()
    PyObject *pFuncLoadModel = PyObject_GetAttrString(pModule, "load_model");
    if (!pFuncLoadModel || !PyCallable_Check(pFuncLoadModel)) {
        PyErr_Print();
        Py_DECREF(pInputTensor);
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    PyObject *pModel = PyObject_CallObject(pFuncLoadModel, NULL);
    Py_DECREF(pFuncLoadModel);
    if (!pModel) {
        PyErr_Print();
        Py_DECREF(pInputTensor);
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    // Call the infer function
    PyObject *pFuncInfer = PyObject_GetAttrString(pModule, "infer");
    if (!pFuncInfer || !PyCallable_Check(pFuncInfer)) {
        PyErr_Print();
        Py_DECREF(pModel);
        Py_DECREF(pInputTensor);
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    PyObject *pArgsInfer = PyTuple_Pack(2, pModel, pInputTensor);
    PyObject *pOutput = PyObject_CallObject(pFuncInfer, pArgsInfer);
    Py_DECREF(pArgsInfer);
    Py_DECREF(pFuncInfer);
    Py_DECREF(pInputTensor);
    Py_DECREF(pModel);
    Py_DECREF(pModule);
    if (!pOutput) {
        PyErr_Print();
        Py_Finalize();
        return -1;
    }

	// Postprocess the inference output
    PyObject *pFuncPostprocess = PyObject_GetAttrString(pModule, "postprocess");
    if (!pFuncPostprocess || !PyCallable_Check(pFuncPostprocess)) {
        PyErr_Print();
        Py_DECREF(pOutput);
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

	// 5 = top 5 results
    PyObject *pArgsPostprocess = PyTuple_Pack(2, pOutput, PyLong_FromLong(5)); 
    PyObject *pResults = PyObject_CallObject(pFuncPostprocess, pArgsPostprocess);
    Py_DECREF(pArgsPostprocess);
    Py_DECREF(pFuncPostprocess);
    Py_DECREF(pOutput);
    if (!pResults) {
        PyErr_Print();
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    // Print the results
    PyObject *pIter = PyObject_GetIter(pResults);
    PyObject *pItem;

    printf("Top-5 Results:\n");
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
    Py_DECREF(pModule);

    // Finalize Python interpreter
    Py_Finalize();

    return 0;
}
