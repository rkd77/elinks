/* Python scripting engine */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "scripting/python/core.h"
#include <Python.h>

#include <stdio.h>
#include <stdlib.h>

#include "elinks.h"

#include "config/home.h"
#include "main/module.h"
#include "util/env.h"
#include "util/file.h"
#include "util/string.h"


PyObject *pDict, *pModule;

void
cleanup_python(struct module *module)
{
	if (Py_IsInitialized()) {
		if (pModule) {
			Py_DECREF(pModule);
		}
		if (PyErr_Occurred()) {
			PyErr_Print();
			PyErr_Clear();
		}
		Py_Finalize();
	}
}

void
init_python(struct module *module)
{
	unsigned char *python_path;

	if (elinks_home)
		python_path = straconcat(elinks_home, ":", CONFDIR, NULL);
	else
		python_path = stracpy(CONFDIR);
	if (!python_path) return;
	env_set("PYTHONPATH", python_path, -1);
	mem_free(python_path);
	Py_Initialize();
	pModule = PyImport_ImportModule("hooks");

	if (pModule) {
		pDict = PyModule_GetDict(pModule);
	} else {
		if (PyErr_Occurred()) {
			PyErr_Print();
			PyErr_Clear();
		}
	}
}
