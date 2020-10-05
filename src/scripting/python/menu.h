#ifndef EL__SCRIPTING_PYTHON_MENU_H
#define EL__SCRIPTING_PYTHON_MENU_H

#include <Python.h>

#ifdef __cplusplus
extern "C" {
#endif

PyObject *python_menu(PyObject *self, PyObject *args, PyObject *kwargs);
extern char python_menu_doc[];

#ifdef __cplusplus
}
#endif

#endif
