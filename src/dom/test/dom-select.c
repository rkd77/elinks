/* Tool for testing the SGML parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "dom/node.h"
#include "dom/select.h"
#include "dom/sgml/parser.h"
#include "dom/stack.h"
#include "util/test.h"

int
main(int argc, char *argv[])
{
	struct dom_node *root;
	struct sgml_parser *parser;
	struct dom_select *select;
	enum sgml_document_type doctype = SGML_DOCTYPE_HTML;
	struct dom_string uri = STATIC_DOM_STRING("dom://test");
	struct dom_string source = STATIC_DOM_STRING("(no source)");
	struct dom_string selector = STATIC_DOM_STRING("(no select)");
	int i;

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (strncmp(arg, "--", 2))
			break;

		arg += 2;

		if (get_test_opt(&arg, "uri", &i, argc, argv, "a URI")) {
			set_dom_string(&uri, arg, strlen(arg));

		} else if (get_test_opt(&arg, "src", &i, argc, argv, "a string")) {
			set_dom_string(&source, arg, strlen(arg));

		} else if (get_test_opt(&arg, "selector", &i, argc, argv, "a string")) {
			set_dom_string(&selector, arg, strlen(arg));

		} else if (!strcmp(arg, "help")) {
			die(NULL);

		} else {
			die("Unknown argument '%s'", arg - 2);
		}
	}

	parser = init_sgml_parser(SGML_PARSER_TREE, doctype, &uri);
	if (!parser) return 1;

	add_dom_stack_tracer(&parser->stack, "sgml-parse: ");

	root = parse_sgml(parser, &source);
	done_sgml_parser(parser);

	if (!root) die("No root node");

	select = init_dom_select(DOM_SELECT_SYNTAX_CSS, &selector);
	if (select) {
		struct dom_node_list *list;

		list = select_dom_nodes(select, root);
		if (list) {
			struct dom_node *node;
			int index;

			foreach_dom_node (list, node, index) {
				DBG("Match");
			}

			mem_free(list);
		}

		done_dom_select(select);
	}

	done_dom_node(root);

	return 0;
}
