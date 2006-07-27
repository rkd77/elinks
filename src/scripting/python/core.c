/* Python scripting engine */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Python.h>

#include <stdio.h>
#include <stdlib.h>

#include "elinks.h"

#include "config/home.h"
#include "main/module.h"
#include "scripting/scripting.h"
#include "scripting/python/core.h"
#include "scripting/python/python.h"
#include "util/env.h"
#include "util/file.h"
#include "util/string.h"


PyObject *pDict = NULL, *pModule = NULL;

/* Error reporting. */

void
alert_python_error(struct session *ses)
{
	unsigned char *msg = "(no traceback available)";
	PyObject *err_type = NULL, *err_value = NULL, *err_traceback = NULL;
	PyObject *tb_module = NULL;
	PyObject *tb_dict;
	PyObject *format_function;
	PyObject *msg_list = NULL;
	PyObject *empty_string = NULL;
	PyObject *join_method = NULL;
	PyObject *msg_string = NULL;
	unsigned char *temp;

	/*
	 * Retrieve the current error indicator and use the format_exception()
	 * function in Python's traceback module to produce an informative
	 * error message. It returns a list of Python string objects.
	 */
	PyErr_Fetch(&err_type, &err_value, &err_traceback);
	PyErr_NormalizeException(&err_type, &err_value, &err_traceback);
	if (!err_traceback) goto end;

	tb_module = PyImport_ImportModule("traceback");
	if (!tb_module) goto end;

	tb_dict = PyModule_GetDict(tb_module);
	format_function = PyDict_GetItemString(tb_dict, "format_exception");
	if (!format_function || !PyCallable_Check(format_function)) goto end;

	msg_list = PyObject_CallFunction(format_function, "OOO",
					 err_type, err_value, err_traceback);
	if (!msg_list) goto end;

	/*
	 * Use the join() method of an empty Python string to join the list
	 * of strings into one Python string containing the entire error
	 * message. Then get the contents of the Python string.
	 */
	empty_string = PyString_FromString("");
	if (!empty_string) goto end;

	join_method = PyObject_GetAttrString(empty_string, "join");
	if (!join_method || !PyCallable_Check(join_method)) goto end;

	msg_string = PyObject_CallFunction(join_method, "O", msg_list);
	if (!msg_string) goto end;

	temp = (unsigned char *)PyString_AsString(msg_string);
	if (temp) msg = temp;

end:
	report_scripting_error(&python_scripting_module, ses, msg);

	Py_XDECREF(err_type);
	Py_XDECREF(err_value);
	Py_XDECREF(err_traceback);
	Py_XDECREF(tb_module);
	Py_XDECREF(msg_list);
	Py_XDECREF(empty_string);
	Py_XDECREF(join_method);
	Py_XDECREF(msg_string);

	/* In case another error occurred while reporting the original error: */
	PyErr_Clear();
}

void
cleanup_python(struct module *module)
{
	if (Py_IsInitialized()) {
		Py_XDECREF(pDict);
		Py_XDECREF(pModule);
		Py_Finalize();
	}
}

void
init_python(struct module *module)
{
	unsigned char *python_path = straconcat(elinks_home, ":", CONFDIR, NULL);

	if (!python_path) return;
	env_set("PYTHONPATH", python_path, -1);
	mem_free(python_path);

	/* Treat warnings as errors so they can be caught and handled;
	 * otherwise they would be printed to stderr.
	 *
	 * NOTE: PySys_ResetWarnOptions() and PySys_AddWarnOption() have been
	 * available and stable for many years but they're not officially
	 * documented as part of Python's public API, so in theory these two
	 * functions might no longer be available in some hypothetical future
	 * version of Python. */
	PySys_ResetWarnOptions();
	PySys_AddWarnOption("error");

	Py_Initialize();
	pModule = PyImport_ImportModule("hooks");

	if (pModule) {
		pDict = PyModule_GetDict(pModule);
		Py_INCREF(pDict);
	} else {
		alert_python_error(NULL);
	}
}
