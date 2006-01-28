
#ifndef EL__SCRIPTING_PYTHON_CORE_H
#define EL__SCRIPTING_PYTHON_CORE_H

struct module;

void init_python(struct module *module);
void cleanup_python(struct module *module);

#endif
