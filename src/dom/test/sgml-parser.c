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
#include "dom/sgml/parser.h"
#include "dom/stack.h"


/* Print the string in a compressed form: a single line with newlines etc.
 * replaced with "\\n" sequence. */
static void
print_compressed_string(struct dom_string *string)
{
	unsigned char escape[2] = "\\";
	size_t pos;

	for (pos = 0; pos < string->length; pos++) {
		unsigned char data = string->string[pos];

		switch (data) {
		case '\n': escape[1] = 'n'; break;
		case '\r': escape[1] = 'r'; break;
		case '\t': escape[1] = 't'; break;
		default:
			putchar(data);
			continue;
		}

		printf("%s", escape);
	}
}

/* Set @string to the value of the given @node, however, with strings
 * compressed and entity references 'expanded'. */
static void
print_dom_node_value(struct dom_node *node)
{
	struct dom_string *value;

	assert(node);

	switch (node->type) {
	case DOM_NODE_ENTITY_REFERENCE:
		/* FIXME: Set to the entity value. */
		printf("%.*s", node->string.length, node->string.string);
		break;

	default:
		value = get_dom_node_value(node);
		if (!value) {
			printf("(no value)");
			return;
		}

		print_compressed_string(value);
	}
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
	struct dom_string *name;
	struct dom_string *id;

	assert(node);

	name	= get_dom_node_name(node);
	id	= get_dom_node_type_name(node->type);

	printf("%.*s %.*s: %.*s -> ",
		get_indent_offset(stack), indent_string,
		id->length, id->string, name->length, name->string);
	print_dom_node_value(node);
	printf("\n");
}

static void
sgml_parser_test_leaf(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string *name;

	assert(node);

	name	= get_dom_node_name(node);

	printf("%.*s %.*s: ",
		get_indent_offset(stack), indent_string,
		name->length, name->string);
	print_dom_node_value(node);
	printf("\n");
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


void die(const char *msg, ...)
{
	va_list args;

	if (msg) {
		va_start(args, msg);
		vfprintf(stderr, msg, args);
		fputs("\n", stderr);
		va_end(args);
	}

	exit(!!NULL);
}

int
main(int argc, char *argv[])
{
	struct dom_node *root;
	struct sgml_parser *parser;
	enum sgml_document_type doctype = SGML_DOCTYPE_HTML;
	struct dom_string uri = INIT_DOM_STRING("dom://test", -1);
	struct dom_string source = INIT_DOM_STRING("(no source)", -1);
	int i;

	for (i = 1; i < argc; i++) {
		char *arg = argv[i];

		if (strncmp(arg, "--", 2))
			break;

		arg += 2;

		if (!strncmp(arg, "uri", 3)) {
			arg += 3;
			if (*arg == '=') {
				arg++;
				set_dom_string(&uri, arg, strlen(arg));
			} else {
				i++;
				if (i >= argc)
					die("--uri expects a URI");
				set_dom_string(&uri, argv[i], strlen(argv[i]));
			}

		} else if (!strncmp(arg, "src", 3)) {
			arg += 3;
			if (*arg == '=') {
				arg++;
				set_dom_string(&source, arg, strlen(arg));
			} else {
				i++;
				if (i >= argc)
					die("--src expects a string");
				set_dom_string(&source, argv[i], strlen(argv[i]));
			}

		} else if (!strcmp(arg, "help")) {
			die(NULL);

		} else {
			die("Unknown argument '%s'", arg - 2);
		}
	}

	parser = init_sgml_parser(SGML_PARSER_STREAM, doctype, &uri);
	if (!parser) return 1;

	add_dom_stack_context(&parser->stack, NULL, &sgml_parser_test_context_info);

	root = parse_sgml(parser, &source);
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
