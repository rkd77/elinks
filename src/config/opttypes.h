#ifndef EL__CONFIG_OPTTYPES_H
#define EL__CONFIG_OPTTYPES_H

#include "config/options.h"
#include "util/string.h"

struct option_type_info {
	unsigned char *name;
	unsigned char *(*cmdline)(struct option *, unsigned char ***, int *);
	unsigned char *(*read)(struct option *, unsigned char **, int *);
	void (*write)(struct option *, struct string *);
	void (*dup)(struct option *, struct option *);
	int (*set)(struct option *, unsigned char *);
	int (*equals)(struct option *, const unsigned char *);
	unsigned char *help_str;
};

/* enum option_type is index in this array */
extern const struct option_type_info option_types[];

extern int commandline;

unsigned char *get_option_type_name(enum option_type type);

#endif
