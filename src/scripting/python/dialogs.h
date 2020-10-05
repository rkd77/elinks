#ifndef EL__SCRIPTING_PYTHON_DIALOGS_H
#define EL__SCRIPTING_PYTHON_DIALOGS_H

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

PyObject *python_info_box(PyObject *self, PyObject *args, PyObject *kwargs);
extern char python_info_box_doc[];
PyObject *python_input_box(PyObject *self, PyObject *args, PyObject *kwargs);
extern char python_input_box_doc[];

#ifdef __cplusplus
}
#endif

#endif
