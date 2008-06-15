
#ifndef EL__SCRIPTING_PYTHON_CORE_H
#define EL__SCRIPTING_PYTHON_CORE_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/* PyString_AsStringAndSize() takes a Py_ssize_t * in Python 2.5 but
 * an int * in Python 2.4.  To be compatible with both, ELinks uses
 * Py_ssize_t and defines it here if necessary.  The public-domain
 * PEP 353 <http://www.python.org/dev/peps/pep-0353/> suggests the
 * following snippet, so we can use the Py_ssize_t name even though
 * Python generally reserves names starting with "Py_".  */
#if PY_VERSION_HEX < 0x02050000 && !defined(PY_SSIZE_T_MIN)
typedef int Py_ssize_t;
#define PY_SSIZE_T_MAX INT_MAX
#define PY_SSIZE_T_MIN INT_MIN
#endif
/* End of PEP 353 snippet.  */

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
