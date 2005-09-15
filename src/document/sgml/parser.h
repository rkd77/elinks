/* $Id: parser.h,v 1.4 2004/09/24 02:11:44 jonas Exp $ */

#ifndef EL__DOCUMENT_SGML_PARSER_H
#define EL__DOCUMENT_SGML_PARSER_H

#include "document/dom/node.h"
#include "document/sgml/sgml.h"
#include "util/scanner.h"

struct cache_entry;
struct document;
struct string;

enum sgml_parser_flags {
	SGML_PARSER_ADD_ELEMENT_ENDS = 1,
};

struct sgml_parser {
	/* The parser flags controls what gets added to the DOM tree */
	enum sgml_parser_flags flags;
	struct sgml_info *info;

	struct document *document;
	struct cache_entry *cache_entry;
	struct dom_node *root;

	struct scanner scanner;
};

struct sgml_parser_state {
	struct sgml_node_info *info;
};

struct dom_node *
parse_sgml(struct cache_entry *cached, struct document *document, struct string *buffer);

#endif
