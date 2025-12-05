/* Python scripting engine */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "scripting/python/pythoninc.h"

#include <osdefs.h>

#include <stdlib.h>

#include "elinks.h"

#include "config/home.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "main/main.h"
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
	ELOG
	const char *msg = "(no traceback available)";
	PyObject *err_type = NULL, *err_value = NULL, *err_traceback = NULL;
	PyObject *tb_module = NULL;
	PyObject *msg_list = NULL;
	PyObject *empty_string = NULL;
	PyObject *msg_string = NULL;
	char *temp;

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
	empty_string = PyUnicode_FromString("");
	if (!empty_string) goto end;

	msg_string = PyObject_CallMethod(empty_string, "join", "O", msg_list);
	if (!msg_string) goto end;

	temp = (char *) PyUnicode_AsUTF8(msg_string);
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
	ELOG
	struct string new_python_path;
	char *old_python_path;
	int result = -1;
	char *xdg_config_home = get_xdg_config_home();

	if (!init_string(&new_python_path)) return result;

	if (xdg_config_home && !add_format_to_string(&new_python_path, "%s%c",
						 xdg_config_home, DELIM))
		goto end;

	if (!add_to_string(&new_python_path, CONFDIR))
		goto end;

	old_python_path = (char *) getenv("PYTHONPATH");
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
	ELOG
	PyObject *imp_module = NULL, *result = NULL;
	int found_hooks = 0;

	/*
	 * Use the find_spec() function in Python's importlib.util module to look for
	 * the hooks module in Python's search path. An ImportError exception
	 * indicates that no such module was found; any other exception will
	 * be reported as an error.
	 */
	imp_module = PyImport_ImportModule("importlib.util");
	if (!imp_module) goto python_error;

	result = PyObject_CallMethod(imp_module, "find_spec", "s", "hooks");
	if (result) {
		found_hooks = (result != Py_None);
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

/*
 * Turn any warnings that aren't filtered into exceptions so they can be caught
 * and handled; otherwise they would be printed to stderr.
 */

static char python_showwarnings_doc[] =
PYTHON_DOCSTRING("showwarnings(message, category, filename, lineno, \
file=None, line=None)\n\
\n\
Report a Python warning as an ELinks scripting error.\n\
\n\
Arguments:\n\
\n\
message -- An instance of the class Warning (or a subclass).\n\
\n\
All other arguments are ignored.\n");

static PyCFunction *
python_showwarning(PyObject *self, PyObject *args, PyObject *kwargs)
{
	ELOG
	PyObject *warning;

	if (!PyTuple_Check(args)) {
		PyErr_Format(PyExc_TypeError, "expected a tuple, got '%s'",
			     args->ob_type->tp_name);
		return NULL;
	}
	warning = PyTuple_GetItem(args, 0);
	if (warning) PyErr_SetObject((PyObject *) warning->ob_type, warning);
	return NULL;
}

static PyMethodDef warning_methods[] = {
	{"showwarning",		(PyCFunction) python_showwarning,
				METH_VARARGS | METH_KEYWORDS,
				python_showwarnings_doc},

	{NULL,			NULL, 0, NULL}
};

/* Replace the standard warnings.showwarning() function. */

static int
replace_showwarning(void)
{
	ELOG
	PyObject *warnings_module = NULL, *module_name = NULL, *module_dict;
	int result = -1;

	warnings_module = PyImport_ImportModule("warnings");
	if (!warnings_module) goto end;
	module_name = PyUnicode_FromString("warnings");
	if (!module_name) goto end;
	module_dict = PyModule_GetDict(warnings_module);
	if (!module_dict) goto end;

	if (add_python_methods(module_dict, module_name, warning_methods) != 0)
		goto end; 

	result = 0;

end:
	Py_XDECREF(warnings_module);
	Py_XDECREF(module_name);
	return result;
}

static PyObject *
python_get_option(PyObject *self, PyObject *args)
{
	const char *name;

	if (!PyArg_ParseTuple(args, "s", &name)) {
		return NULL;
	}
	struct option *opt = get_opt_rec(config_options, (char *)name);

	if (!opt) {
		return NULL;
	}
	/* Convert to an appropriate Python type */
	switch (opt->type) {
	case OPT_BOOL:
		return PyBool_FromLong(opt->value.number);
	case OPT_INT:
		return PyLong_FromLong(opt->value.number);
	case OPT_LONG:
		return PyLong_FromLongLong(opt->value.big_number);
	case OPT_STRING:
		return PyUnicode_FromString(opt->value.string);
	case OPT_CODEPAGE:
	{
		const char *cp_name;

		cp_name = get_cp_config_name(opt->value.number);
		return PyUnicode_FromString(cp_name);
	}
	case OPT_LANGUAGE:
	{
		const char *lang;

#ifdef ENABLE_NLS
		lang = language_to_name(current_language);
#else
		lang = "System";
#endif
		return PyUnicode_FromString(lang);
	}
	case OPT_COLOR:
	{
		color_T color;
		char hexcolor[8];
		const char *strcolor;

		color = opt->value.color;
		strcolor = get_color_string(color, hexcolor);

		return PyUnicode_FromString(strcolor);
	}
	case OPT_COMMAND:
	default:
		Py_RETURN_NONE;
	}
}

static PyObject *
python_set_option(PyObject *self, PyObject *args)
{
	const char *name;
	PyObject *v;

	if (!PyArg_ParseTuple(args, "sO", &name, &v)) {
		return NULL;
	}
	/* Get option record */
	struct option *opt = get_opt_rec(config_options, (char *)name);
	if (opt == NULL) {
		Py_RETURN_NONE;
	}

	/* Set option */
	switch (opt->type) {
	case OPT_BOOL:
	{
		/* option_types[OPT_BOOL].set expects a long even though it
		 * saves the value to opt->value.number, which is an int.  */
		long value = PyLong_AsLong(v);
		option_types[opt->type].set(opt, (char *)(&value));
		break;
	}
	case OPT_INT:
	case OPT_LONG:
	{
		/* option_types[OPT_INT].set expects a long even though it
		 * saves the value to opt->value.number, which is an int.
		 * option_types[OPT_LONG].set of course wants a long too.  */
		long value = PyLong_AsLong(v);
		option_types[opt->type].set(opt, (char *)(&value));
		break;
	}
	case OPT_STRING:
	case OPT_CODEPAGE:
	case OPT_LANGUAGE:
	case OPT_COLOR:
		if (!PyUnicode_Check(v)) {
			Py_RETURN_NONE;
		}
		PyObject *utf8 = PyUnicode_AsEncodedString(v, "utf-8", NULL);
		const char *text = PyBytes_AsString(utf8);
		if (text) {
			option_types[opt->type].set(opt, (char *)text);
		}
		Py_XDECREF(utf8);
		break;
	default:
		Py_RETURN_NONE;
	}
	/* Call hook */
	option_changed(python_ses, opt);
	Py_RETURN_NONE;
}

/* Module-level documentation for the Python interpreter's elinks module. */

char python_get_option_doc[] =
PYTHON_DOCSTRING("get_option(option)\n\
\n\
Return value of given option.\n\
\n\
Arguments:\n\
\n\
option -- A string containing option name.\n");

char python_set_option_doc[] =
PYTHON_DOCSTRING("set_option(option, value) -> None\n\
\n\
Set value for given option.\n\
\n\
Arguments:\n\
\n\
option -- A string containing option name.\n\
value -- A value\n");

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
home -- A string containing the pathname of the ~/.config/elinks directory, or\n\
        None if ELinks has no configuration directory.\n");

static PyMethodDef python_methods[] = {
	{"info_box",		(PyCFunction)python_info_box,
				METH_VARARGS | METH_KEYWORDS,
				python_info_box_doc},

	{"input_box",		(PyCFunction)python_input_box,
				METH_VARARGS | METH_KEYWORDS,
				python_input_box_doc},

	{"current_document",	python_current_document,
				METH_NOARGS,
				python_current_document_doc},

	{"current_header",	python_current_header,
				METH_NOARGS,
				python_current_header_doc},

	{"current_link_url",	python_current_link_url,
				METH_NOARGS,
				python_current_link_url_doc},

	{"current_title",	python_current_title,
				METH_NOARGS,
				python_current_title_doc},

	{"current_url",		python_current_url,
				METH_NOARGS,
				python_current_url_doc},

	{"bind_key",		(PyCFunction)python_bind_key,
				METH_VARARGS | METH_KEYWORDS,
				python_bind_key_doc},

	{"get_option",		python_get_option,
				METH_VARARGS,
				python_get_option_doc},

	{"load",		python_load,
				METH_VARARGS,
				python_load_doc},

	{"menu",		(PyCFunction)python_menu,
				METH_VARARGS | METH_KEYWORDS,
				python_menu_doc},

	{"open",		(PyCFunction)python_open,
				METH_VARARGS | METH_KEYWORDS,
				python_open_doc},

	{"set_option",		python_set_option,
				METH_VARARGS,
				python_set_option_doc},

	{NULL,			NULL, 0, NULL}
};

static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	"elinks",            /* m_name */
	module_doc,          /* m_doc */
	-1,                  /* m_size */
	python_methods,      /* m_methods */
	NULL,                /* m_reload */
	NULL,                /* m_traverse */
	NULL,                /* m_clear */
	NULL,                /* m_free */
};

static int
add_constant(PyObject *dict, const char *key, int value)
{
	ELOG
	PyObject *constant = PyLong_FromLong(value);
	int result;

	if (!constant) return -1;
	result = PyDict_SetItemString(dict, key, constant);
	Py_DECREF(constant);

	return result;
}

PyMODINIT_FUNC
PyInit_elinks(void)
{
	ELOG
	PyObject *elinks_module, *module_dict, *module_name;
	char *xdg_config_home = get_xdg_config_home();

	if (replace_showwarning() != 0) {
		goto python_error;
	}

	keybindings = PyDict_New();
	if (!keybindings) {
		goto python_error;
	}

	elinks_module = PyModule_Create(&moduledef);
	if (!elinks_module) {
		goto python_error;
	}

	/* If @xdg_config_home is NULL, Py_BuildValue() returns a None reference. */
	if (PyModule_AddObject(elinks_module, "home",
			       Py_BuildValue("s", xdg_config_home)) != 0) {
		goto python_error;
	}

	python_elinks_err = PyErr_NewException("elinks.error", NULL, NULL);
	if (!python_elinks_err) {
		goto python_error;
	}

	if (PyModule_AddObject(elinks_module, "error", python_elinks_err) != 0) {
		goto python_error;
	}

	module_dict = PyModule_GetDict(elinks_module);
	if (!module_dict) {
		goto python_error;
	}

	add_constant(module_dict, "MENU_LINK", PYTHON_MENU_LINK);
	add_constant(module_dict, "MENU_TAB", PYTHON_MENU_TAB);

	module_name = PyUnicode_FromString("elinks");
	if (!module_name) {
		goto python_error;
	}
	return elinks_module;

python_error:
	alert_python_error();
	return NULL;
}


void
init_python(struct module *module)
{
	ELOG
	if (set_python_search_path() != 0) {
		return;
	}
	PyImport_AppendInittab("elinks", PyInit_elinks);
	Py_Initialize();

	if (!hooks_module_exists()) {
		return;
	}

	python_hooks = PyImport_ImportModule("hooks");
	if (!python_hooks) {
		goto python_error;
	}

	return;

python_error:
	alert_python_error();
}

void
cleanup_python(struct module *module)
{
	ELOG
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
	ELOG
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

