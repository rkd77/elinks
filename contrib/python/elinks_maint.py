"""Additional Python code for ELinks maintainers.

This module is intended for ELinks maintainers. If you modify or add to
the Python APIs in src/scripting/python/*.c and/or contrib/python/hooks.py,
you should update the accompanying docstrings to reflect your changes and
then generate a new version of the file doc/python.txt (which serves as a
reference manual for the browser's Python APIs). The embedded interpreter
can use introspection to regenerate the python.txt document for you; just
copy this file into your ~/.elinks directory and add something like the
following to ~/.elinks/hooks.py:

import elinks_maint
elinks.bind_key('F2', elinks_maint.generate_python_txt)

"""

import inspect
import tempfile
import types

preface = """\
Python programmers can customize the behavior of ELinks by creating a Python
hooks module. The embedded Python interpreter provides an internal module
called elinks that can be used by the hooks module to create keystroke
bindings for Python code, obtain information about the document being
viewed, display simple dialog boxes and menus, load documents into the
ELinks cache, or display documents to the user. These two modules are
described below.

"""

module_template = """
MODULE
    %s - %s

DESCRIPTION
%s

FUNCTIONS
%s
"""

separator = '-' * 78 + '\n'

def document_modules(*modules):
    """Format the internal documentation found in one or more Python modules."""
    output = []
    for module in modules:
        name, doc, namespace = module.__name__, module.__doc__, module.__dict__
        if not name or not namespace:
            continue
        try:
            summary, junk, description = doc.rstrip().split('\n', 2)
        except:
            summary, description = '?', '(no description available)'
        functions = document_functions(namespace)
        output.append(module_template % (name, summary, indent(description),
                                         indent(functions)))
    return separator.join(output)

def document_functions(namespace):
    """Format the internal documentation for all functions in a namespace."""
    objects = namespace.items()
    objects.sort()
    output = []
    for name, object in objects:
        if name.startswith('_'):
            continue
        species = type(object)
        if species == types.BuiltinFunctionType:
            args = '(...)'
        elif species == types.FunctionType:
            args = inspect.formatargspec(*inspect.getargspec(object))
        else:
            continue
        description = inspect.getdoc(object)
        output.append('%s%s\n%s\n' % (name, args, indent(description)))
    return '\n'.join(output)

def generate_python_txt():
    """Generate documentation for the hooks and elinks modules."""
    import elinks
    import hooks

    # Remove anything that doesn't belong in the API documentation.
    #
    hooks_api_functions = (
        'follow_url_hook',
        'goto_url_hook',
        'pre_format_html_hook',
        'proxy_for_hook',
        'quit_hook',
    )
    for key in hooks.__dict__.keys():
        if key not in hooks_api_functions and not key.startswith('_'):
            del hooks.__dict__[key]
    hooks.__doc__ = hooks.__doc__.replace('Example Python', 'Python')

    # Generate the documentation.
    #
    try:
        output = separator.join((preface, document_modules(hooks, elinks)))
    finally:
        # Restore the hooks module to a sane state.
        reload(hooks)

    # View the documentation.
    #
    path = write_tempfile(output)
    elinks.open(path)

def indent(text):
    """Return indented text."""
    indent = ' ' * 4
    return '\n'.join([indent + line for line in text.split('\n')])

def write_tempfile(text):
    """Write a string to a temporary file and return the file's name."""
    output = tempfile.NamedTemporaryFile(prefix='elinks_maint', suffix='.txt')
    output.write(text)
    output.flush()
    _tempfiles[text] = output
    return output.name

# Temp files are stashed in this dictionary to prevent them from being closed
# before ELinks has a chance to read them; they will be automatically deleted
# when the dictionary is garbage-collected at exit time.
#
_tempfiles = {}
