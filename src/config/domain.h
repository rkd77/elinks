#ifndef EL__CONFIG_DOMAIN_H
#define EL__CONFIG_DOMAIN_H

#include "config/options.h"
#include "util/lists.h"

struct session;


struct domain_tree {
	LIST_HEAD(struct domain_tree);

	struct option *tree;

	int len;

	unsigned char name[1]; /* Must be at end of struct. */
};

extern LIST_OF(struct domain_tree) domain_trees;

struct option *get_domain_tree(unsigned char *);

struct option *get_domain_option_from_session(unsigned char *,
                                              struct session *);

void done_domain_trees(void);

#endif
