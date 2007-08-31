/* Domain-specific options infrastructure */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/domain.h"
#include "config/options.h"
#include "protocol/uri.h"
#include "session/location.h"
#include "session/session.h"
#include "util/lists.h"
#include "viewer/text/vs.h"


INIT_LIST_OF(struct domain_tree, domain_trees);


/* Look for the option with the given name in all domain shadow-trees that
 * match the given domain-name.  Return the option from the the shadow tree
 * that best matches the given domain name. */
static struct option *
get_domain_option(unsigned char *domain_name, int domain_len,
                  unsigned char *name)
{
	struct option *opt, *longest_match_opt = NULL;
	struct domain_tree *longest_match = NULL;
	struct domain_tree *domain;

	assert(domain_name);
	assert(*domain_name);

	foreach (domain, domain_trees)
		if ((!longest_match || domain->len > longest_match->len)
		    && is_in_domain(domain->name, domain_name, domain_len)
		    && (opt = get_opt_rec_real(domain->tree, name))) {
			longest_match = domain;
			longest_match_opt = opt;
		}

	return longest_match_opt;
}

struct option *
get_domain_option_from_session(unsigned char *name, struct session *ses)
{
	struct uri *uri;

	assert(ses);
	assert(name);

	if (!have_location(ses))
		return NULL;

	uri = cur_loc(ses)->vs.uri;
	if (!uri->host || !uri->hostlen)
		return NULL;

	return get_domain_option(uri->host, uri->hostlen, name);
}

/* Return the shadow shadow tree for the given domain name, and
 * if the domain does not yet have a shadow tree, create it. */
struct option *
get_domain_tree(unsigned char *domain_name)
{
	struct domain_tree *domain;
	int domain_len;

	assert(domain_name);
	assert(*domain_name);

	foreach (domain, domain_trees)
		if (!strcasecmp(domain->name, domain_name))
			return domain->tree;

	domain_len = strlen(domain_name);
	/* One byte is reserved for domain in struct domain_tree. */
	domain = mem_alloc(sizeof(*domain) + domain_len);
	if (!domain) return NULL;

	domain->tree = copy_option(config_options, CO_SHALLOW
	                                            | CO_NO_LISTBOX_ITEM);
	if (!domain->tree) {
		mem_free(domain);
		return NULL;
	}

	memcpy(domain->name, domain_name, domain_len + 1);
	domain->len = domain_len;

	add_to_list(domain_trees, domain);

	return domain->tree;
}

void
done_domain_trees(void)
{
	struct domain_tree *domain, *next;

	foreachsafe (domain, next, domain_trees) {
		delete_option(domain->tree);
		domain->tree = NULL;
		del_from_list(domain);
		mem_free(domain);
	}
}
