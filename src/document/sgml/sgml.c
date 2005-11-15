/* SGML generics */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/dom/node.h"
#include "document/sgml/sgml.h"
#include "util/error.h"
#include "util/string.h"


int
sgml_info_strcmp(const void *key_, const void *node_)
{
	struct dom_node *key = (struct dom_node *) key_;
	struct sgml_node_info *node = (struct sgml_node_info *) node_;
	int length = int_min(key->length, node->length);
	int string_diff = strncasecmp(key->string, node->string, length);
	int length_diff = key->length - node->length;

	/* If the lengths or strings don't match strncasecmp() does the job
	 * else return which ever is bigger. */

	return (!length_diff || string_diff) ? string_diff : length_diff;
}
