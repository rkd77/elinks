/* Dialog boxes for Python. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Python.h>

#include "elinks.h"

#include "bfu/inpfield.h"
#include "bfu/msgbox.h"
#include "intl/gettext/libintl.h"
#include "scripting/python/core.h"
#include "session/session.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"

/* Python interface for displaying information to the user. */

static char python_info_box_doc[] =
PYTHON_DOCSTRING("info_box(text[, title]) -> None\n\
\n\
Display information to the user in a dialog box.\n\
\n\
Arguments:\n\
\n\
text -- The text to be displayed in the dialog box. This argument can\n\
        be a string or any object that has a string representation as\n\
        returned by str(object).\n\
\n\
Optional arguments:\n\
\n\
title -- A string containing a title for the dialog box. By default\n\
        the string \"Info\" is used.\n");

static PyObject *
python_info_box(PyObject *self, PyObject *args, PyObject *kwargs)
{
	/* [gettext_accelerator_context(python_info_box)] */
	unsigned char *title = N_("Info");
	PyObject *object, *string_object;
	unsigned char *text;
	static char *kwlist[] = {"text", "title", NULL};

	if (!python_ses) {
		PyErr_SetString(python_elinks_err, "No session");
		return NULL;
	}

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|s:info_box", kwlist,
					 &object, &title))
		return NULL;

	assert(object);
	if_assert_failed {
		PyErr_SetString(python_elinks_err, N_("Internal error"));
		return NULL;
	}

	/*
	 * Get a string representation of the object, then copy that string's
	 * contents.
	 */
	string_object = PyObject_Str(object);
	if (!string_object) return NULL;
	text = (unsigned char *) PyString_AS_STRING(string_object);
	if (!text) {
		Py_DECREF(string_object);
		return NULL;
	}
	text = stracpy(text);
	Py_DECREF(string_object);
	if (!text) goto mem_error;

	title = stracpy(title);
	if (!title) goto free_text;

	(void) msg_box(python_ses->tab->term, getml(title, (void *) NULL),
		       MSGBOX_NO_INTL | MSGBOX_SCROLLABLE | MSGBOX_FREE_TEXT,
		       title, ALIGN_LEFT,
		       text,
		       NULL, 1,
		       MSG_BOX_BUTTON(N_("~OK"), NULL, B_ENTER | B_ESC));

	Py_INCREF(Py_None);
	return Py_None;

free_text:
	mem_free(text);

mem_error:
	return PyErr_NoMemory();
}

struct python_input_callback_hop {
	struct session *ses;
	PyObject *callback;
};

/*
 * C wrapper that invokes Python callbacks for input_dialog() OK button.
 *
 * This is also used indirectly for the Cancel button, with a NULL @text
 * argument. See invoke_input_cancel_callback() below.
 */

static void
invoke_input_ok_callback(void *data, unsigned char *text)
{
	struct python_input_callback_hop *hop = data;
	struct session *saved_python_ses = python_ses;
	PyObject *result;

	assert(hop && hop->callback);
	if_assert_failed return;

	python_ses = hop->ses;

	/* If @text is NULL, the "s" format will create a None reference. */
	result = PyObject_CallFunction(hop->callback, "s", text);
	if (result)
		Py_DECREF(result);
	else
		alert_python_error();

	Py_DECREF(hop->callback);
	mem_free(hop);

	python_ses = saved_python_ses;
}

/* C wrapper that invokes Python callbacks for input_dialog() cancel button. */

static void
invoke_input_cancel_callback(void *data)
{
	invoke_input_ok_callback(data, NULL);
}

/* Python interface for getting input from the user. */

static char python_input_box_doc[] =
PYTHON_DOCSTRING(
"input_box(prompt, callback, title=\"User dialog\", initial=\"\") -> None\n\
\n\
Display a dialog box to prompt for user input.\n\
\n\
Arguments:\n\
\n\
prompt -- A string containing a prompt for the dialog box.\n\
callback -- A callable object to be called after the dialog is\n\
        finished. It will be called with a single argument, which\n\
        will be either a string provided by the user or else None\n\
        if the user canceled the dialog.\n\
\n\
Optional keyword arguments:\n\
\n\
title -- A string containing a title for the dialog box. By default\n\
        the string \"User dialog\" is used.\n\
initial -- A string containing an initial value for the text entry\n\
        field. By default the entry field is initially empty.\n");

static PyObject *
python_input_box(PyObject *self, PyObject *args, PyObject *kwargs)
{
	unsigned char *prompt;
	PyObject *callback;
	unsigned char *title = N_("User dialog");
	unsigned char *initial = NULL;
	struct python_input_callback_hop *hop;
	static char *kwlist[] = {"prompt", "callback", "title", "initial", NULL};

	if (!python_ses) {
		PyErr_SetString(python_elinks_err, "No session");
		return NULL;
	}

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sO|ss:input_box",
					 kwlist, &prompt, &callback, &title,
					 &initial))
		return NULL;

	assert(prompt && callback && title);
	if_assert_failed {
		PyErr_SetString(python_elinks_err, N_("Internal error"));
		return NULL;
	}

	prompt = stracpy(prompt);
	if (!prompt) goto mem_error;

	title = stracpy(title);
	if (!title) goto free_prompt;

	if (initial) {
		initial = stracpy(initial);
		if (!initial) goto free_title;
	}

	hop = mem_alloc(sizeof(*hop));
	if (!hop) goto free_initial;
	hop->ses = python_ses;
	hop->callback = callback;
	Py_INCREF(callback);

	input_dialog(python_ses->tab->term,
		     getml(prompt, (void *) title, (void *) initial, (void *) NULL),
		     title, prompt,
		     hop, NULL,
		     MAX_STR_LEN, initial, 0, 0, NULL,
		     invoke_input_ok_callback,
		     invoke_input_cancel_callback);

	Py_INCREF(Py_None);
	return Py_None;

free_initial:
	mem_free_if(initial);

free_title:
	mem_free(title);

free_prompt:
	mem_free(prompt);

mem_error:
	return PyErr_NoMemory();
}

static PyMethodDef dialogs_methods[] = {
	{"info_box",		(PyCFunction) python_info_box,
				METH_VARARGS | METH_KEYWORDS,
				python_info_box_doc},

	{"input_box",		(PyCFunction) python_input_box,
				METH_VARARGS | METH_KEYWORDS,
				python_input_box_doc},

	{NULL,			NULL, 0, NULL}
};

int
python_init_dialogs_interface(PyObject *dict, PyObject *name)
{
	return add_python_methods(dict, name, dialogs_methods);
}
