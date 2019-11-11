#ifndef EL__SCRIPTING_PYTHON_DOCUMENT_H
#define EL__SCRIPTING_PYTHON_DOCUMENT_H

#include <Python.h>

PyObject *python_current_document(PyObject *self, PyObject *args);
extern char python_current_document_doc[];
PyObject *python_current_header(PyObject *self, PyObject *args);
extern char python_current_header_doc[];
PyObject *python_current_link_url(PyObject *self, PyObject *args);
extern char python_current_link_url_doc[];
PyObject *python_current_title(PyObject *self, PyObject *args);
extern char python_current_title_doc[];
PyObject *python_current_url(PyObject *self, PyObject *args);
extern char python_current_url_doc[];

#endif
