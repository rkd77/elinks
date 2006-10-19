
#ifndef EL__SCRIPTING_PYTHON_CORE_H
#define EL__SCRIPTING_PYTHON_CORE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

struct module;

extern struct session *python_ses;
extern PyObject *python_elinks_err;

void alert_python_error(void);

void init_python(struct module *module);
void cleanup_python(struct module *module);

int add_python_methods(PyObject *dict, PyObject *name, PyMethodDef *methods);

#ifndef CONFIG_SMALL
#define PYTHON_DOCSTRING(str) str
#else
#define PYTHON_DOCSTRING(str) ""
#endif

#endif
