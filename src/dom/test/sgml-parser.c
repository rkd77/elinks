/* Tool for testing the SGML parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "dom/node.h"
#include "dom/sgml/parser.h"
#include "dom/stack.h"


/* Compress a string to a single line with newlines etc. replaced with "\\n"
 * sequence. */
static inline unsigned char *
compress_string(unsigned char *string, unsigned int length)
{
	struct string buffer;
	unsigned char escape[2] = "\\";

	if (!init_string(&buffer)) return NULL;

	for (; length > 0; string++, length--) {
		unsigned char *bytes = string;

		if (*string == '\n' || *string == '\r' || *string == '\t') {
			bytes	  = escape;
			escape[1] = *string == '\n' ? 'n'
				  : (*string == '\r' ? 'r' : 't');
		}

		add_bytes_to_string(&buffer, bytes, bytes == escape ? 2 : 1);
	}

	return buffer.source;
}

/* Set @string to the value of the given @node, however, with strings
 * compressed and entity references 'expanded'. */
static void
set_enhanced_dom_node_value(struct dom_string *string, struct dom_node *node)
{
	struct dom_string *value;

	assert(node);

	memset(string, 0, sizeof(*string));

	switch (node->type) {
	case DOM_NODE_ENTITY_REFERENCE:
		/* FIXME: Set to the entity value. */
		string->string = null_or_stracpy(string->string);
		break;

	default:
		value = get_dom_node_value(node);
		if (!value) {
			set_dom_string(string, NULL, 0);
			return;
		}

		string->string = compress_string(value->string, value->length);
	}

	string->length = string->string ? strlen(string->string) : 0;
}

static unsigned char indent_string[] =
	"                                                                    ";

#define get_indent_offset(stack) \
	((stack)->depth < sizeof(indent_string)/2 ? (stack)->depth * 2 : sizeof(indent_string))

static void
sgml_parser_test_tree(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string *value = &node->string;
	struct dom_string *name = get_dom_node_name(node);

	printf("%.*s %.*s: %.*s\n",
		get_indent_offset(stack), indent_string,
		name->length, name->string,
		value->length, value->string);
}

static void
sgml_parser_test_id_leaf(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string value;
	struct dom_string *name;
	struct dom_string *id;

	assert(node);

	name	= get_dom_node_name(node);
	id	= get_dom_node_type_name(node->type);
	set_enhanced_dom_node_value(&value, node);

	printf("%.*s %.*s: %.*s -> %.*s\n",
		get_indent_offset(stack), indent_string,
		id->length, id->string, name->length, name->string,
		value.length, value.string);

	if (is_dom_string_set(&value))
		done_dom_string(&value);
}

static void
sgml_parser_test_leaf(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string *name;
	struct dom_string value;

	assert(node);

	name	= get_dom_node_name(node);
	set_enhanced_dom_node_value(&value, node);

	printf("%.*s %.*s: %.*s\n",
		get_indent_offset(stack), indent_string,
		name->length, name->string,
		value.length, value.string);

	if (is_dom_string_set(&value))
		done_dom_string(&value);
}

static void
sgml_parser_test_branch(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string *name;
	struct dom_string *id;

	assert(node);

	name	= get_dom_node_name(node);
	id	= get_dom_node_type_name(node->type);

	printf("%.*s %.*s: %.*s\n",
		get_indent_offset(stack), indent_string,
		id->length, id->string, name->length, name->string);
}

struct dom_stack_context_info sgml_parser_test_context_info = {
	/* Object size: */			0,
	/* Push: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ sgml_parser_test_branch,
		/* DOM_NODE_ATTRIBUTE		*/ sgml_parser_test_id_leaf,
		/* DOM_NODE_TEXT		*/ sgml_parser_test_leaf,
		/* DOM_NODE_CDATA_SECTION	*/ sgml_parser_test_id_leaf,
		/* DOM_NODE_ENTITY_REFERENCE	*/ sgml_parser_test_id_leaf,
		/* DOM_NODE_ENTITY		*/ sgml_parser_test_id_leaf,
		/* DOM_NODE_PROC_INSTRUCTION	*/ sgml_parser_test_id_leaf,
		/* DOM_NODE_COMMENT		*/ sgml_parser_test_leaf,
		/* DOM_NODE_DOCUMENT		*/ sgml_parser_test_tree,
		/* DOM_NODE_DOCUMENT_TYPE	*/ sgml_parser_test_id_leaf,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ sgml_parser_test_id_leaf,
		/* DOM_NODE_NOTATION		*/ sgml_parser_test_id_leaf,
	},
	/* Pop: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ NULL,
		/* DOM_NODE_ATTRIBUTE		*/ NULL,
		/* DOM_NODE_TEXT		*/ NULL,
		/* DOM_NODE_CDATA_SECTION	*/ NULL,
		/* DOM_NODE_ENTITY_REFERENCE	*/ NULL,
		/* DOM_NODE_ENTITY		*/ NULL,
		/* DOM_NODE_PROC_INSTRUCTION	*/ NULL,
		/* DOM_NODE_COMMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT_TYPE	*/ NULL,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ NULL,
		/* DOM_NODE_NOTATION		*/ NULL,
	}
};

int
main(int argc, char *argv[])
{
	struct dom_node *root;
	struct sgml_parser *parser;
	enum sgml_document_type doctype = SGML_DOCTYPE_HTML;
	struct dom_string uri = INIT_DOM_STRING("dom://test", -1);
	struct dom_string buffer = INIT_DOM_STRING("<html><body><p>Hello World!</p></body></html>", -1);

	parser = init_sgml_parser(SGML_PARSER_STREAM, doctype, &uri);
	if (!parser) return 1;

	add_dom_stack_context(&parser->stack, NULL, &sgml_parser_test_context_info);

	root = parse_sgml(parser, &buffer);
	if (root) {
		assert(parser->stack.depth == 1);

		get_dom_stack_top(&parser->stack)->immutable = 0;
		/* For SGML_PARSER_STREAM this will free the DOM
		 * root node. */
		pop_dom_node(&parser->stack);
	}

	done_sgml_parser(parser);

	return 0;
}
