/* Document loading for Python. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <Python.h>

#include "elinks.h"

#include "cache/cache.h"
#include "intl/gettext/libintl.h"
#include "network/connection.h"
#include "network/state.h"
#include "protocol/uri.h"
#include "scripting/python/core.h"
#include "session/download.h"
#include "session/session.h"
#include "session/task.h"
#include "util/error.h"
#include "util/memory.h"

struct python_load_uri_callback_hop {
	struct session *ses;
	PyObject *callback;
};

/* C wrapper that invokes Python callbacks for load_uri(). */

static void
invoke_load_uri_callback(struct download *download, void *data)
{
	struct python_load_uri_callback_hop *hop = data;
	struct session *saved_python_ses = python_ses;

	assert(download);
	if_assert_failed {
		if (hop && hop->callback) {
			Py_DECREF(hop->callback);
		}
		mem_free_if(hop);
		return;
	}

	if (is_in_progress_state(download->state)) return;

	assert(hop && hop->callback);
	if_assert_failed {
		mem_free(download);
		mem_free_if(hop);
		return;
	}

	if (download->cached) {
		PyObject *result;
		struct fragment *f = get_cache_fragment(download->cached);

		python_ses = hop->ses;

		result = PyObject_CallFunction(hop->callback, "ss#",
					       download->cached->head,
					       f ? f->data : NULL,
					       f ? f->length : 0);
		if (result)
			Py_DECREF(result);
		else
			alert_python_error();
	}

	Py_DECREF(hop->callback);
	mem_free(hop);
	mem_free(download);

	python_ses = saved_python_ses;
}

/* Python interface for loading a document. */

static char python_load_doc[] =
PYTHON_DOCSTRING("load(url, callback) -> None\n\
\n\
Load a document into the ELinks cache and pass its contents to a\n\
callable object.\n\
\n\
Arguments:\n\
\n\
url -- A string containing the URL to load.\n\
callback -- A callable object to be called after the document has\n\
        been loaded. It will be called with two arguments: the first\n\
        will be a string representing the document's header, or None\n\
        if it has no header; the second will be a string representing\n\
        the document's body, or None if it has no body.\n");

static PyObject *
python_load(PyObject *self, PyObject *args)
{
	unsigned char *uristring;
	PyObject *callback;
	struct uri *uri;
	struct download *download;
	struct python_load_uri_callback_hop *hop;

	if (!python_ses) {
		PyErr_SetString(python_elinks_err, "No session");
		return NULL;
	}

	if (!PyArg_ParseTuple(args, "sO:load", &uristring, &callback))
		return NULL;

	assert(uristring && callback);
	if_assert_failed {
		PyErr_SetString(python_elinks_err, N_("Internal error"));
		return NULL;
	}

	uri = get_translated_uri(uristring, python_ses->tab->term->cwd);
	if (!uri) {
		PyErr_SetString(python_elinks_err, N_("Bad URL syntax"));
		return NULL;
	}

	download = mem_alloc(sizeof(*download));
	if (!download) goto mem_error;

	hop = mem_alloc(sizeof(*hop));
	if (!hop) goto free_download;
	hop->ses = python_ses;
	hop->callback = callback;
	Py_INCREF(callback);

	download->data = hop;
	download->callback = (download_callback_T *) invoke_load_uri_callback;
	if (load_uri(uri, NULL, download, PRI_MAIN, CACHE_MODE_NORMAL, -1) != 0) {
		PyErr_SetString(python_elinks_err,
				get_state_message(download->state,
						  python_ses->tab->term));
		done_uri(uri);
		return NULL;
	}

	done_uri(uri);
	Py_INCREF(Py_None);
	return Py_None;

free_download:
	mem_free(download);

mem_error:
	done_uri(uri);
	return PyErr_NoMemory();
}


static PyMethodDef load_methods[] = {
	{"load",		python_load,
				METH_VARARGS,
				python_load_doc},

	{NULL,			NULL, 0, NULL}
};

int
python_init_load_interface(PyObject *dict, PyObject *name)
{
	return add_python_methods(dict, name, load_methods);
}
