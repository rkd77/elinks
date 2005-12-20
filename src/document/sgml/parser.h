
#ifndef EL__DOCUMENT_SGML_PARSER_H
#define EL__DOCUMENT_SGML_PARSER_H

#include "document/dom/node.h"
#include "document/dom/stack.h"
#include "document/sgml/sgml.h"
#include "util/scanner.h"

struct string;
struct uri;

enum sgml_parser_type {
	/* The first one is a DOM tree builder. */
	SGML_PARSER_TREE,
	/* The second one will simply push nodes on the stack, not building a
	 * DOM tree. This interface is similar to that of SAX (Simple API for
	 * XML) where events are fired when nodes are entered and exited. It is
	 * useful when you are not actually interested in the DOM tree, but can
	 * do all processing in a stream-like manner, such as when highlighting
	 * HTML code. */
	SGML_PARSER_STREAM,
};

struct sgml_parser {
	enum sgml_parser_type type;

	struct sgml_info *info;

	struct uri *uri;
	struct dom_node *root;
	void *data;

	struct scanner scanner;
	struct dom_stack stack;
};

struct sgml_parser_state {
	struct sgml_node_info *info;
	/* This is used by the DOM source renderer for highlighting the
	 * end-tag of an element. */
	struct scanner_token end_token;
};

struct sgml_parser *
init_sgml_parser(enum sgml_parser_type type, enum sgml_document_type doctype,
		 void *data, struct uri *uri,
		 struct dom_stack_callbacks *callbacks);

void done_sgml_parser(struct sgml_parser *parser);

struct dom_node *parse_sgml(struct sgml_parser *parser, struct string *buffer);

#endif
