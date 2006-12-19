/* Python scripting engine */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Python.h>
#include <osdefs.h>

#include <stdlib.h>

#include "elinks.h"

#include "config/home.h"
#include "main/module.h"
#include "scripting/python/core.h"
#include "scripting/python/dialogs.h"
#include "scripting/python/document.h"
#include "scripting/python/keybinding.h"
#include "scripting/python/load.h"
#include "scripting/python/menu.h"
#include "scripting/python/open.h"
#include "scripting/python/python.h"
#include "scripting/scripting.h"
#include "session/session.h"
#include "util/env.h"
#include "util/string.h"

struct session *python_ses = NULL;

PyObject *python_elinks_err = NULL;
PyObject *python_hooks = NULL;

/* Error reporting. */

void
alert_python_error(void)
{
	unsigned char *msg = "(no traceback available)";
	PyObject *err_type = NULL, *err_value = NULL, *err_traceback = NULL;
	PyObject *tb_module = NULL;
	PyObject *msg_list = NULL;
	PyObject *empty_string = NULL;
	PyObject *msg_string = NULL;
	unsigned char *temp;

	/*
	 * Retrieve the current error indicator and use the format_exception()
	 * function in Python's traceback module to produce an informative
	 * error message. It returns a list of Python string objects.
	 */
	PyErr_Fetch(&err_type, &err_value, &err_traceback);
	PyErr_NormalizeException(&err_type, &err_value, &err_traceback);
	if (!err_type) goto end;

	tb_module = PyImport_ImportModule("traceback");
	if (!tb_module) goto end;

	msg_list = PyObject_CallMethod(tb_module, "format_exception", "OOO",
				       err_type,
				       err_value ? err_value : Py_None,
				       err_traceback ? err_traceback : Py_None);
	if (!msg_list) goto end;

	/*
	 * Use the join() method of an empty Python string to join the list
	 * of strings into one Python string containing the entire error
	 * message. Then get the contents of the Python string.
	 */
	empty_string = PyString_FromString("");
	if (!empty_string) goto end;

	msg_string = PyObject_CallMethod(empty_string, "join", "O", msg_list);
	if (!msg_string) goto end;

	temp = (unsigned char *) PyString_AsString(msg_string);
	if (temp) msg = temp;

end:
	report_scripting_error(&python_scripting_module, python_ses, msg);

	Py_XDECREF(err_type);
	Py_XDECREF(err_value);
	Py_XDECREF(err_traceback);
	Py_XDECREF(tb_module);
	Py_XDECREF(msg_list);
	Py_XDECREF(empty_string);
	Py_XDECREF(msg_string);

	/* In case another error occurred while reporting the original error: */
	PyErr_Clear();
}

/* Prepend ELinks directories to Python's search path. */

static int
set_python_search_path(void)
{
	struct string new_python_path;
	unsigned char *old_python_path;
	int result = -1;

	if (!init_string(&new_python_path)) return result;

	if (elinks_home && !add_format_to_string(&new_python_path, "%s%c",
						 elinks_home, DELIM))
		goto end;

	if (!add_to_string(&new_python_path, CONFDIR))
		goto end;

	old_python_path = (unsigned char *) getenv("PYTHONPATH");
	if (old_python_path && !add_format_to_string(&new_python_path, "%c%s",
						     DELIM, old_python_path))
		goto end;

	result = env_set("PYTHONPATH", new_python_path.source, -1);

end:
	done_string(&new_python_path);
	return result;
}

/* Determine whether or not a user-supplied hooks module exists. */

static int
hooks_module_exists(void)
{
	PyObject *imp_module = NULL, *result = NULL;
	int found_hooks = 0;

	/*
	 * Use the find_module() function in Python's imp module to look for
	 * the hooks module in Python's search path. An ImportError exception
	 * indicates that no such module was found; any other exception will
	 * be reported as an error.
	 */
	imp_module = PyImport_ImportModule("imp");
	if (!imp_module) goto python_error;

	result = PyObject_CallMethod(imp_module, "find_module", "s", "hooks");
	if (result) {
		found_hooks = 1;
		goto end;
	} else if (PyErr_ExceptionMatches(PyExc_ImportError)) {
		PyErr_Clear();
		goto end;
	}

python_error:
	alert_python_error();

end:
	Py_XDECREF(imp_module);
	Py_XDECREF(result);
	return found_hooks;
}

/* Module-level documentation for the Python interpreter's elinks module. */

static char module_doc[] =
PYTHON_DOCSTRING("Interface to the ELinks web browser.\n\
\n\
Functions:\n\
\n\
bind_key() -- Bind a keystroke to a callable object.\n\
current_document() -- Return the body of the document being viewed.\n\
current_header() -- Return the header of the document being viewed.\n\
current_link_url() -- Return the URL of the currently selected link.\n\
current_title() -- Return the title of the document being viewed.\n\
current_url() -- Return the URL of the document being viewed.\n\
info_box() -- Display information to the user.\n\
input_box() -- Prompt for user input.\n\
load() -- Load a document into the ELinks cache.\n\
menu() -- Display a menu.\n\
open() -- View a document.\n\
\n\
Exception classes:\n\
\n\
error -- Errors internal to ELinks.\n\
\n\
Other public objects:\n\
\n\
home -- A string containing the pathname of the ~/.elinks directory, or\n\
        None if ELinks has no configuration directory.\n");

void
init_python(struct module *module)
{
	PyObject *elinks_module, *module_dict, *module_name;

	if (set_python_search_path() != 0) return;

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

	if (!hooks_module_exists()) return;

	elinks_module = Py_InitModule3("elinks", NULL, module_doc);
	if (!elinks_module) goto python_error;

	/* If @elinks_home is NULL, Py_BuildValue() returns a None reference. */
	if (PyModule_AddObject(elinks_module, "home",
			       Py_BuildValue("s", elinks_home)) != 0)
		goto python_error;

	python_elinks_err = PyErr_NewException("elinks.error", NULL, NULL);
	if (!python_elinks_err) goto python_error;

	if (PyModule_AddObject(elinks_module, "error", python_elinks_err) != 0)
		goto python_error;

	module_dict = PyModule_GetDict(elinks_module);
	if (!module_dict) goto python_error;
	module_name = PyString_FromString("elinks");
	if (!module_name) goto python_error;

	if (python_init_dialogs_interface(module_dict, module_name) != 0
	    || python_init_document_interface(module_dict, module_name) != 0
	    || python_init_keybinding_interface(module_dict, module_name) != 0
	    || python_init_load_interface(module_dict, module_name) != 0
	    || python_init_menu_interface(module_dict, module_name) != 0
	    || python_init_open_interface(module_dict, module_name) != 0)
		goto python_error;

	python_hooks = PyImport_ImportModule("hooks");
	if (!python_hooks) goto python_error;

	return;

python_error:
	alert_python_error();
}

void
cleanup_python(struct module *module)
{
	if (Py_IsInitialized()) {
		PyObject *temp;

		python_done_keybinding_interface();

		/* This is equivalent to Py_CLEAR(), but it works with older
		 * versions of Python predating that macro: */
		temp = python_hooks;
		python_hooks = NULL;
		Py_XDECREF(temp);

		Py_Finalize();
	}
}

/* Add methods to a Python extension module. */

int
add_python_methods(PyObject *dict, PyObject *name, PyMethodDef *methods)
{
	PyMethodDef *method;

	for (method = methods; method && method->ml_name; method++) {
		PyObject *function = PyCFunction_NewEx(method, NULL, name);
		int result;

		if (!function) return -1;
		result = PyDict_SetItemString(dict, method->ml_name, function);
		Py_DECREF(function);
		if (result != 0) return -1;
	}
	return 0;
}
