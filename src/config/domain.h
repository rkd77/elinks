#ifndef EL__CONFIG_DOMAIN_H
#define EL__CONFIG_DOMAIN_H

#include "config/options.h"
#include "util/lists.h"

#ifdef __cplusplus
extern "C" {
#endif

struct session;


struct domain_tree {
	LIST_HEAD(struct domain_tree);

	struct option *tree;

	int len;

	char name[1]; /* Must be at end of struct. */
};

extern LIST_OF(struct domain_tree) domain_trees;

struct option *get_domain_tree(char *);

struct option *get_domain_option_from_session(const char *,
                                              struct session *);

void done_domain_trees(void);

#ifdef __cplusplus
}
#endif

#endif
