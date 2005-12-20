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

/* Backend includes: */

#include "document/sgml/html/html.h"
#include "document/sgml/rss/rss.h"


int
sgml_info_strcmp(const void *key_, const void *node_)
{
	struct dom_node *key = (struct dom_node *) key_;
	struct sgml_node_info *node = (struct sgml_node_info *) node_;

	return dom_string_casecmp(&key->string, &node->string);
}

struct sgml_info *sgml_info[SGML_DOCTYPES] = {
	&sgml_html_info,
	&sgml_rss_info,
};

struct sgml_info *
get_sgml_info(enum sgml_document_type doctype)
{
	return doctype < SGML_DOCTYPES ? sgml_info[doctype] : NULL;
}
