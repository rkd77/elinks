/* Keystroke bindings for Python. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Python.h>

#include <stdarg.h>
#include <string.h>

#include "elinks.h"

#include "config/kbdbind.h"
#include "intl/gettext/libintl.h"
#include "main/event.h"
#include "scripting/python/core.h"
#include "session/session.h"
#include "util/error.h"
#include "util/string.h"

static PyObject *keybindings = NULL;

/* C wrapper that invokes Python callbacks for bind_key_to_event_name(). */

static enum evhook_status
invoke_keybinding_callback(va_list ap, void *data)
{
	PyObject *callback = data;
	struct session *saved_python_ses = python_ses;
	PyObject *result;

	python_ses = va_arg(ap, struct session *);

	result = PyObject_CallFunction(callback, NULL);
	if (result)
		Py_DECREF(result);
	else
		alert_python_error();

	python_ses = saved_python_ses;

	return EVENT_HOOK_STATUS_NEXT;
}

/* Check that a keymap name is valid. */

static int
keymap_is_valid(const unsigned char *keymap)
{
	enum keymap_id keymap_id;

	for (keymap_id = 0; keymap_id < KEYMAP_MAX; ++keymap_id)
		if (!strcmp(keymap, get_keymap_name(keymap_id)))
			break;
	return (keymap_id != KEYMAP_MAX);
}

/* Python interface for binding keystrokes to callable objects. */

static char python_bind_key_doc[] =
PYTHON_DOCSTRING("bind_key(keystroke, callback[, keymap]) -> None\n\
\n\
Bind a keystroke to a callable object.\n\
\n\
Arguments:\n\
\n\
keystroke -- A string containing a keystroke. The syntax for\n\
        keystrokes is described in the elinkskeys(5) man page.\n\
callback -- A callable object to be called when the keystroke is\n\
        typed. It will be called without any arguments.\n\
\n\
Optional arguments:\n\
\n\
keymap -- A string containing the name of a keymap. Valid keymap\n\
          names can be found in the elinkskeys(5) man page. By\n\
          default the \"main\" keymap is used.\n");

static PyObject *
python_bind_key(PyObject *self, PyObject *args, PyObject *kwargs)
{
	const unsigned char *keystroke;
	PyObject *callback;
	unsigned char *keymap = "main";
	PyObject *key_tuple;
	PyObject *old_callback;
	struct string event_name;
	int event_id;
	unsigned char *error_msg;
	static char *kwlist[] = {"keystroke", "callback", "keymap", NULL};

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|s:bind_key", kwlist,
					 &keystroke, &callback, &keymap))
		return NULL;

	assert(keystroke && callback && keymap);
	if_assert_failed {
		PyErr_SetString(python_elinks_err, N_("Internal error"));
		return NULL;
	}

	if (!keymap_is_valid(keymap)) {
		PyErr_Format(python_elinks_err, "%s \"%s\"",
			     N_("Unrecognised keymap"), keymap);
		return NULL;
	}

	/*
	 * The callback object needs to be kept alive for as long as the
	 * keystroke is bound, so we stash a reference to it in a dictionary.
	 * We don't need to use the dictionary to find callbacks; its sole
	 * purpose is to prevent these objects from being garbage-collected
	 * by the Python interpreter.
	 *
	 * If binding the key fails for any reason after this point then
	 * we'll need to restore the dictionary to its previous state, which
	 * is temporarily preserved in @old_callback.
	 */
	key_tuple = Py_BuildValue("ss", keymap, keystroke);
	if (!key_tuple)
		return NULL;
	old_callback = PyDict_GetItem(keybindings, key_tuple);
	Py_XINCREF(old_callback);
	if (PyDict_SetItem(keybindings, key_tuple, callback) != 0) {
		Py_DECREF(key_tuple);
		Py_XDECREF(old_callback);
		return NULL;
	}

	if (!init_string(&event_name)) {
		PyErr_NoMemory();
		goto rollback;
	}
	if (!add_format_to_string(&event_name, "python-func %p", callback)) {
		PyErr_SetFromErrno(python_elinks_err);
		done_string(&event_name);
		goto rollback;
	}
	event_id = bind_key_to_event_name(keymap, keystroke, event_name.source,
					  &error_msg);
	done_string(&event_name);
	if (error_msg) {
		PyErr_SetString(python_elinks_err, error_msg);
		goto rollback;
	}

	event_id = register_event_hook(event_id, invoke_keybinding_callback, 0,
				       callback);
	if (event_id == EVENT_NONE) {
		PyErr_SetString(python_elinks_err,
				N_("Error registering event hook"));
		goto rollback;
	}

	Py_DECREF(key_tuple);
	Py_XDECREF(old_callback);

	Py_INCREF(Py_None);
	return Py_None;

rollback:
	/*
	 * If an error occurred, try to restore the keybindings dictionary
	 * to its previous state.
	 */
	if (old_callback) {
		(void) PyDict_SetItem(keybindings, key_tuple, old_callback);
		Py_DECREF(old_callback);
	} else {
		(void) PyDict_DelItem(keybindings, key_tuple);
	}

	Py_DECREF(key_tuple);
	return NULL;
}

static PyMethodDef keybinding_methods[] = {
	{"bind_key",		(PyCFunction) python_bind_key,
				METH_VARARGS | METH_KEYWORDS,
				python_bind_key_doc},

	{NULL,			NULL, 0, NULL}
};

int
python_init_keybinding_interface(PyObject *dict, PyObject *name)
{
	keybindings = PyDict_New();
	if (!keybindings) return -1;

	return add_python_methods(dict, name, keybinding_methods);
}

void
python_done_keybinding_interface(void)
{
	PyObject *temp;

	/* This is equivalent to Py_CLEAR(), but it works with older
	 * versions of Python predating that macro: */
	temp = keybindings;
	keybindings = NULL;
	Py_XDECREF(temp);
}
