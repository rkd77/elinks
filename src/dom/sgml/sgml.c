/* SGML generics */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "dom/node.h"
#include "dom/sgml/sgml.h"
#include "dom/string.h"
#include "util/error.h"

/* Backend includes: */

#include "dom/sgml/docbook/docbook.h"
#include "dom/sgml/html/html.h"
#include "dom/sgml/rss/rss.h"
#include "dom/sgml/xbel/xbel.h"


int
sgml_info_strcmp(const void *key_, const void *node_)
{
	struct dom_node *key = (struct dom_node *) key_;
	struct sgml_node_info *node = (struct sgml_node_info *) node_;

	return dom_string_casecmp(&key->string, &node->string);
}

struct sgml_info *sgml_info[SGML_DOCTYPES] = {
	&sgml_docbook_info,
	&sgml_html_info,
	&sgml_rss_info,
	&sgml_xbel_info,
};

struct sgml_info *
get_sgml_info(enum sgml_document_type doctype)
{
	return doctype < SGML_DOCTYPES ? sgml_info[doctype] : NULL;
}
