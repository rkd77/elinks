/* Document viewing for Python. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Python.h>

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "protocol/uri.h"
#include "scripting/python/core.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "util/error.h"

/* Python interface for viewing a document. */

static char python_open_doc[] =
PYTHON_DOCSTRING("open(url, new_tab=False, background=False) -> None\n\
\n\
View a document in either the current tab or a new tab.\n\
\n\
Arguments:\n\
\n\
url -- A string containing the URL to view.\n\
\n\
Optional keyword arguments:\n\
\n\
new_tab -- By default the URL is opened in the current tab. If this\n\
        argument's value is the boolean True then the URL is instead\n\
        opened in a new tab.\n\
background -- By default a new tab is opened in the foreground. If\n\
        this argument's value is the boolean True then a new tab is\n\
        instead opened in the background. This argument is ignored\n\
        unless new_tab's value is True.\n");

static PyObject *
python_open(PyObject *self, PyObject *args, PyObject *kwargs)
{
	unsigned char *url;
	int new_tab = 0, background = 0;
	struct uri *uri;
	static char *kwlist[] = {"url", "new_tab", "background", NULL};

	if (!python_ses) {
		PyErr_SetString(python_elinks_err, "No session");
		return NULL;
	}

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|ii:open",
					 kwlist, &url,
					 &new_tab, &background))
		return NULL;

	assert(url);
	if_assert_failed {
		PyErr_SetString(python_elinks_err, N_("Internal error"));
		return NULL;
	}

	uri = get_translated_uri(url, python_ses->tab->term->cwd);
	if (!uri) {
		PyErr_SetString(python_elinks_err, N_("Bad URL syntax"));
		return NULL;
	}

	if (new_tab)
		open_uri_in_new_tab(python_ses, uri, background, 0);
	else
		goto_uri(python_ses, uri);

	done_uri(uri);

	Py_INCREF(Py_None);
	return Py_None;
}

static PyMethodDef open_methods[] = {
	{"open",		(PyCFunction) python_open,
				METH_VARARGS | METH_KEYWORDS,
				python_open_doc},

	{NULL,			NULL, 0, NULL}
};

int
python_init_open_interface(PyObject *dict, PyObject *name)
{
	return add_python_methods(dict, name, open_methods);
}
