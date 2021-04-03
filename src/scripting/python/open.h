#ifndef EL__SCRIPTING_PYTHON_OPEN_H
#define EL__SCRIPTING_PYTHON_OPEN_H

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

PyObject *python_open(PyObject *self, PyObject *args, PyObject *kwargs);
extern char python_open_doc[];

#ifdef __cplusplus
}
#endif

#endif
