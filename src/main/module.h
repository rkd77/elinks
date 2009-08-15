#ifndef EL__MAIN_MODULE_H
#define EL__MAIN_MODULE_H

#include "config/options.h"
#include "main/event.h"

/* The module record */

struct module {
	/* The name of the module. It needs to be unique in its class (ie. in
	 * the scope of root modules or submodules of one parent module). */
	unsigned char *name;

	/* The options that should be registered for this module.
	 * The table should end with NULL_OPTION_INFO. */
	union option_info *options;

	/* The event hooks that should be registered for this module.
	 * The table should end with NULL_EVENT_HOOK_INFO. */
	struct event_hook_info *hooks;

	/* Any submodules that this module contains. Order matters
	 * since it is garanteed that initialization will happen in
	 * the specified order and teardown in the reverse order.
	 * The table should end with NULL. */
	struct module **submodules;

	/* User data for the module. Undefined purpose. */
	void *data;

	/* Lifecycle functions */

	/* This function should initialize the module. */
	void (*init)(struct module *module);

	/* This function should shutdown the module. */
	void (*done)(struct module *module);
};

#define struct_module(name, options, hooks, submods, data, init, done) \
	{ name, options, hooks, submods, data, init, done }

#define foreach_module(module, modules, i)			\
	for (i = 0, module = modules ? modules[i] : NULL;	\
	     module;						\
	     i++, module = modules[i])

/* The module table has to be NULL terminates */
static inline int
sizeof_modules(struct module **modules)
{
	struct module *module;
	int i;

	foreach_module (module, modules, i) {
		; /* m33p */
	}

	return i - 1;
}

#define foreachback_module(module, modules, i)			\
	for (i = sizeof_modules(modules);			\
	     i >= 0 && (module = modules[i]);			\
	     i--)

/* Interface for handling single modules */

void register_module_options(struct module *module);
void unregister_module_options(struct module *module);

void init_module(struct module *module);
void done_module(struct module *module);

/* Interface for handling builtin modules */

/* Builtin modules are initialised only when not connecting to a master
 * terminal. */
extern struct module *builtin_modules[];

/* Main modules are initialised earlier and are not listed in Help -> About. */
extern struct module *main_modules[];

void register_modules_options(struct module *modules[]);
void unregister_modules_options(struct module *modules[]);

void init_modules(struct module *modules[]);
void done_modules(struct module *modules[]);

#endif
