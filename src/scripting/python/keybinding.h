#ifndef EL__SCRIPTING_PYTHON_KEYBINDING_H
#define EL__SCRIPTING_PYTHON_KEYBINDING_H

#include <Python.h>

int python_init_keybinding_interface(PyObject *dict, PyObject *name);
void python_done_keybinding_interface(void);

#endif
