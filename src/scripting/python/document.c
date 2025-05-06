/* Information about current document and current link for Python. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "scripting/python/pythoninc.h"

#include "elinks.h"

#include "cache/cache.h"
#include "document/document.h"
#include "document/view.h"
#include "scripting/python/core.h"
#include "scripting/python/document.h"
#include "session/session.h"

/* Python interface to get the current document's body. */

char python_current_document_doc[] =
PYTHON_DOCSTRING("current_document() -> string or None\n\
\n\
If a document is being viewed, return its body; otherwise return None.\n");

PyObject *
python_current_document(PyObject *self, PyObject *args)
{
	ELOG
	if (python_ses && python_ses->doc_view
	    && python_ses->doc_view->document) {
		struct cache_entry *cached = python_ses->doc_view->document->cached;
		struct fragment *f = (struct fragment *)(cached ? cached->frag.next : NULL);

		if (f) return PyUnicode_FromStringAndSize(f->data, f->length);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/* Python interface to get the current document's header. */

char python_current_header_doc[] =
PYTHON_DOCSTRING("current_header() -> string or None\n\
\n\
If a document is being viewed and it has a header, return the header;\n\
otherwise return None.\n");

PyObject *
python_current_header(PyObject *self, PyObject *args)
{
	ELOG
	if (python_ses && python_ses->doc_view
	    && python_ses->doc_view->document) {
		struct cache_entry *cached = python_ses->doc_view->document->cached;

		if (cached && cached->head)
			return PyUnicode_FromString(cached->head);
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/* Python interface to get the currently-selected link's URL. */

char python_current_link_url_doc[] =
PYTHON_DOCSTRING("current_link_url() -> string or None\n\
\n\
If a link is selected, return its URL; otherwise return None.\n");

PyObject *
python_current_link_url(PyObject *self, PyObject *args)
{
	ELOG
	char url[MAX_STR_LEN];

	if (python_ses && get_current_link_url(python_ses, url, MAX_STR_LEN))
		return PyUnicode_FromString(url);

	Py_INCREF(Py_None);
	return Py_None;
}

/* Python interface to get the current document's title. */

char python_current_title_doc[] =
PYTHON_DOCSTRING("current_title() -> string or None\n\
\n\
If a document is being viewed, return its title; otherwise return None.\n");

PyObject *
python_current_title(PyObject *self, PyObject *args)
{
	ELOG
	char title[MAX_STR_LEN];

	if (python_ses && get_current_title(python_ses, title, MAX_STR_LEN))
		return PyUnicode_FromString(title);

	Py_INCREF(Py_None);
	return Py_None;
}

/* Python interface to get the current document's URL. */

char python_current_url_doc[] =
PYTHON_DOCSTRING("current_url() -> string or None\n\
\n\
If a document is being viewed, return its URL; otherwise return None.\n");

PyObject *
python_current_url(PyObject *self, PyObject *args)
{
	ELOG
	char url[MAX_STR_LEN];

	if (python_ses && get_current_url(python_ses, url, MAX_STR_LEN))
		return PyUnicode_FromString(url);

	Py_INCREF(Py_None);
	return Py_None;
}
