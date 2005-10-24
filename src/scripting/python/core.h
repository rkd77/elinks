
#ifndef EL__SCRIPTING_PYTHON_CORE_H
#define EL__SCRIPTING_PYTHON_CORE_H

#include <Python.h>

struct module;

extern PyObject *pDict, *pModule;

void init_python(struct module *module);
void cleanup_python(struct module *module);

#endif
