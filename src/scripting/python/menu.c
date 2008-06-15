/* Simple menus for Python. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Python.h>

#include "elinks.h"

#include "bfu/menu.h"
#include "document/document.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "scripting/python/core.h"
#include "session/session.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/view.h"

/* C wrapper that invokes Python callbacks for menu items. */

static void
invoke_menu_callback(struct terminal *term, void *data, void *ses)
{
	PyObject *callback = data;
	struct session *saved_python_ses = python_ses;
	PyObject *result;

	python_ses = ses;

	result = PyObject_CallFunction(callback, NULL);
	if (result)
		Py_DECREF(result);
	else
		alert_python_error();

	Py_DECREF(callback);

	python_ses = saved_python_ses;
}

enum python_menu_type {
	PYTHON_MENU_DEFAULT,
	PYTHON_MENU_LINK,
	PYTHON_MENU_TAB,
	PYTHON_MENU_MAX
};

/* Python interface for displaying simple menus. */

static char python_menu_doc[] =
PYTHON_DOCSTRING("menu(items[, type]) -> None\n\
\n\
Display a menu.\n\
\n\
Arguments:\n\
\n\
items -- A sequence of tuples. Each tuple must have two elements: a\n\
        string containing the name of a menu item, and a callable\n\
        object that will be called without any arguments if the user\n\
        selects that menu item.\n\
\n\
Optional arguments:\n\
\n\
type -- A constant specifying the type of menu to display. By default\n\
        the menu is displayed at the top of the screen, but if this\n\
        argument's value is the constant elinks.MENU_TAB then the menu\n\
        is displayed in the same location as the ELinks tab menu. If\n\
        its value is the constant elinks.MENU_LINK then the menu is\n\
        displayed in the same location as the ELinks link menu and is\n\
        not displayed unless a link is currently selected.\n");

static PyObject *
python_menu(PyObject *self, PyObject *args, PyObject *kwargs)
{
	PyObject *items;
	enum python_menu_type menu_type = PYTHON_MENU_DEFAULT;
	Py_ssize_t length;
	int i;
	struct menu_item *menu;
	struct memory_list *ml = NULL;
	static char *kwlist[] = {"items", "type", NULL};

	if (!python_ses) {
		PyErr_SetString(python_elinks_err, "No session");
		return NULL;
	}

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|i:menu",
					 kwlist, &items, &menu_type))
		return NULL;

	assert(items);
	if_assert_failed {
		PyErr_SetString(python_elinks_err, N_("Internal error"));
		return NULL;
	}

	if (!PySequence_Check(items)) {
		PyErr_SetString(PyExc_TypeError, "Argument must be a sequence");
		return NULL;
	}
	length = PySequence_Length(items);
	if (length == -1 || length > INT_MAX) return NULL;
	else if (length == 0) goto success;

	if (menu_type < 0 || menu_type >= PYTHON_MENU_MAX) {
		PyErr_Format(python_elinks_err, "%s %d",
			     N_("Bad number"), menu_type);
		return NULL;

	} else if (menu_type == PYTHON_MENU_LINK) {
		if (!get_current_link(current_frame(python_ses)))
			goto success;

	} else if (menu_type == PYTHON_MENU_TAB
		   && python_ses->status.show_tabs_bar) {
		int y;

		if (python_ses->status.show_tabs_bar_at_top)
			y = python_ses->status.show_title_bar;
		else
			y = python_ses->tab->term->height - length
			    - python_ses->status.show_status_bar - 2;

		set_window_ptr(python_ses->tab, python_ses->tab->xpos,
			       int_max(y, 0));

	} else {
		set_window_ptr(python_ses->tab, 0, 0);
	}

	menu = new_menu(FREE_LIST | FREE_TEXT | NO_INTL);
	if (!menu) return PyErr_NoMemory();

	/*
	 * Keep track of all the memory we allocate so we'll be able to free
	 * it in case any error prevents us from displaying the menu.
	 */
	ml = getml(menu, (void *) NULL);
	if (!ml) {
		mem_free(menu);
		return PyErr_NoMemory();
	}

	for (i = 0; i < length; i++) {
		PyObject *tuple = PySequence_GetItem(items, i);
		PyObject *name, *callback;
		unsigned char *contents;

		if (!tuple) goto error;

		if (!PyTuple_Check(tuple)) {
			Py_DECREF(tuple);
			PyErr_SetString(PyExc_TypeError,
					"Argument must be sequence of tuples");
			goto error;
		}
		name = PyTuple_GetItem(tuple, 0);
		callback = PyTuple_GetItem(tuple, 1);
		Py_DECREF(tuple);
		if (!name || !callback) goto error;

		contents = (unsigned char *) PyString_AsString(name);
		if (!contents) goto error;

		contents = stracpy(contents);
		if (!contents) {
			PyErr_NoMemory();
			goto error;
		}
		add_one_to_ml(&ml, contents);

		/*
		 * FIXME: We need to increment the reference counts for
		 * callbacks so they won't be garbage-collected by the Python
		 * interpreter before they're called. But for any callback
		 * that isn't called (because the user doesn't select the
		 * corresponding menu item) we'll never have an opportunity
		 * to decrement the reference count again, so this code leaks
		 * references. It probably can't be fixed without changes to
		 * the menu machinery in bfu/menu.c, e.g. to call an arbitrary
		 * clean-up function when a menu is destroyed.
		 *
		 * The good news is that in a typical usage case, where the
		 * callback objects wouldn't be garbage-collected anyway until
		 * the Python interpreter exits, this makes no difference at
		 * all. But it's not strictly correct, and it could leak memory
		 * in more elaborate usage where callback objects are created
		 * and thrown away on the fly.
		 */
		Py_INCREF(callback);
		add_to_menu(&menu, contents, NULL, ACT_MAIN_NONE,
			    invoke_menu_callback, callback, 0);
	}

	do_menu(python_ses->tab->term, menu, python_ses, 1);

success:
	mem_free_if(ml);

	Py_INCREF(Py_None);
	return Py_None;

error:
	freeml(ml);
	return NULL;
}

static PyMethodDef menu_methods[] = {
	{"menu",		(PyCFunction) python_menu,
				METH_VARARGS | METH_KEYWORDS,
				python_menu_doc},

	{NULL,			NULL, 0, NULL}
};

static int
add_constant(PyObject *dict, const char *key, int value)
{
	PyObject *constant = PyInt_FromLong(value);
	int result;

	if (!constant) return -1;
	result = PyDict_SetItemString(dict, key, constant);
	Py_DECREF(constant);

	return result;
}

int
python_init_menu_interface(PyObject *dict, PyObject *name)
{
	if (add_constant(dict, "MENU_LINK", PYTHON_MENU_LINK) != 0) return -1;
	if (add_constant(dict, "MENU_TAB", PYTHON_MENU_TAB) != 0) return -1;

	return add_python_methods(dict, name, menu_methods);
}
