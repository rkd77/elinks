#ifndef EL__CONFIG_OPTTYPES_H
#define EL__CONFIG_OPTTYPES_H

#include "config/options.h"
#include "util/string.h"

#ifdef __cplusplus
extern "C" {
#endif

struct option_type_info {
	const char *name;
	const char *(*cmdline)(struct option *, char ***, int *);
	char *(*read2)(struct option *, char **, int *);
	void (*write2)(struct option *, struct string *);
	void (*dup)(struct option *, struct option *, int);
	int (*set)(struct option *, char *);
	int (*equals)(struct option *, const char *);
	const char *help_str;
};

/* enum option_type is index in this array */
extern const struct option_type_info option_types[];

extern int commandline;

const char *get_option_type_name(enum option_type type);

#ifdef __cplusplus
}
#endif

#endif
