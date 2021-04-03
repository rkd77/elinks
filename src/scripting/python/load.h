#ifndef EL__SCRIPTING_PYTHON_LOAD_H
#define EL__SCRIPTING_PYTHON_LOAD_H

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

PyObject *python_load(PyObject *self, PyObject *args);
extern char python_load_doc[];

#ifdef __cplusplus
}
#endif

#endif
