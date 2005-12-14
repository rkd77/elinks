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

	return dom_string_casecmp(&key->string, &node->string);
}
