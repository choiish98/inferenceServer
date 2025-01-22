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

    // Load the model by calling load_model()
    PyObject *pFuncLoadModel = PyObject_GetAttrString(pModule, "load_model");
    if (!pFuncLoadModel || !PyCallable_Check(pFuncLoadModel)) {
        PyErr_Print();
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    PyObject *pModel = PyObject_CallObject(pFuncLoadModel, NULL);
    Py_DECREF(pFuncLoadModel);
    if (!pModel) {
        PyErr_Print();
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    // Create a dummy input tensor (for demonstration)
    PyObject *pFuncInfer = PyObject_GetAttrString(pModule, "infer");
    if (!pFuncInfer || !PyCallable_Check(pFuncInfer)) {
        PyErr_Print();
        Py_DECREF(pModel);
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    PyObject *pInputTensor = Py_BuildValue("((fff)(fff)(fff))",
                                           0.1, 0.2, 0.3,
                                           0.4, 0.5, 0.6,
                                           0.7, 0.8, 0.9);
    if (!pInputTensor) {
        PyErr_Print();
        Py_DECREF(pFuncInfer);
        Py_DECREF(pModel);
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    PyObject *pArgs = PyTuple_Pack(2, pModel, pInputTensor);
    if (!pArgs) {
        PyErr_Print();
        Py_DECREF(pInputTensor);
        Py_DECREF(pFuncInfer);
        Py_DECREF(pModel);
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    PyObject *pOutput = PyObject_CallObject(pFuncInfer, pArgs);
    Py_DECREF(pFuncInfer);
    Py_DECREF(pArgs);
    if (!pOutput) {
        PyErr_Print();
        Py_DECREF(pInputTensor);
        Py_DECREF(pModel);
        Py_DECREF(pModule);
        Py_Finalize();
        return -1;
    }

    // Print the output tensor
    PyObject_Print(pOutput, stdout, 0);
    printf("\n");

    Py_DECREF(pOutput);
    Py_DECREF(pInputTensor);
    Py_DECREF(pModel);
    Py_DECREF(pModule);

    // Finalize Python interpreter
    Py_Finalize();

    return 0;
}
