#ifndef EL__SCRIPTING_PYTHON_KEYBINDING_H
#define EL__SCRIPTING_PYTHON_KEYBINDING_H

#include <Python.h>

extern PyObject *keybindings;
PyObject *python_bind_key(PyObject *self, PyObject *args, PyObject *kwargs);
extern char python_bind_key_doc[];
void python_done_keybinding_interface(void);

#endif
