/* Tool for testing the SGML parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "dom/configuration.h"
#include "dom/node.h"
#include "dom/sgml/dump.h"
#include "dom/sgml/parser.h"
#include "dom/stack.h"
#include "util/test.h"


unsigned int number_of_lines = 0;

static int
update_number_of_lines(struct dom_stack *stack)
{
	struct sgml_parser *parser = stack->contexts[0]->data;
	int lines;

	if (!(parser->flags & SGML_PARSER_COUNT_LINES))
		return 0;

	lines = get_sgml_parser_line_number(parser);
	if (number_of_lines < lines)
		number_of_lines = lines;

	return 1;
}

/* Print the string in a compressed form: a single line with newlines etc.
 * replaced with "\\n" sequence. */
static void
print_compressed_string(struct dom_string *string)
{
	unsigned char escape[3] = { '\\', '?', 0 };
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
	(((stack)->depth < sizeof(indent_string)/2 ? (stack)->depth * 2 : sizeof(indent_string)) - 2)


static void
print_indent(struct dom_stack *stack)
{
	printf("%.*s", (int) get_indent_offset(stack), indent_string);
}

static enum dom_code
sgml_parser_test_tree(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string *value = &node->string;
	struct dom_string *name = get_dom_node_name(node);

	/* Always print the URI for identification. */
	if (update_number_of_lines(stack))
		return DOM_CODE_OK;

	print_indent(stack);
	printf("%.*s: %.*s\n",
		name->length, name->string,
		value->length, value->string);

	return DOM_CODE_OK;
}

static enum dom_code
sgml_parser_test_id_leaf(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string *name;
	struct dom_string *id;

	assert(node);

	if (update_number_of_lines(stack))
		return DOM_CODE_OK;

	name	= get_dom_node_name(node);
	id	= get_dom_node_type_name(node->type);

	print_indent(stack);
	printf("%.*s: %.*s -> ",
		id->length, id->string,
		name->length, name->string);
	print_dom_node_value(node);
	printf("\n");

	return DOM_CODE_OK;
}

static enum dom_code
sgml_parser_test_leaf(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string *name;

	assert(node);

	if (update_number_of_lines(stack))
		return DOM_CODE_OK;

	name	= get_dom_node_name(node);

	print_indent(stack);
	printf("%.*s: ",
		name->length, name->string);
	print_dom_node_value(node);
	printf("\n");

	return DOM_CODE_OK;
}

static enum dom_code
sgml_parser_test_branch(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_string *name;
	struct dom_string *id;

	assert(node);

	if (update_number_of_lines(stack))
		return DOM_CODE_OK;

	name	= get_dom_node_name(node);
	id	= get_dom_node_type_name(node->type);

	print_indent(stack);
	printf("%.*s: %.*s\n",
		id->length, id->string, name->length, name->string);

	return DOM_CODE_OK;
}

static enum dom_code
sgml_parser_test_end(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct sgml_parser *parser = stack->contexts[0]->data;

	if ((parser->flags & SGML_PARSER_COUNT_LINES)
	    && !(parser->flags & SGML_PARSER_DETECT_ERRORS)) {
		printf("%d\n", number_of_lines);
	}

	return DOM_CODE_OK;
}

struct dom_stack_context_info sgml_parser_test_context_info = {
	/* Object size: */			0,
	/* Push: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ sgml_parser_test_branch,
		/* DOM_NODE_ATTRIBUTE		*/ sgml_parser_test_id_leaf,
		/* DOM_NODE_TEXT		*/ sgml_parser_test_leaf,
		/* DOM_NODE_CDATA_SECTION	*/ sgml_parser_test_leaf,
		/* DOM_NODE_ENTITY_REFERENCE	*/ sgml_parser_test_branch,
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
		/* DOM_NODE_DOCUMENT		*/ sgml_parser_test_end,
		/* DOM_NODE_DOCUMENT_TYPE	*/ NULL,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ NULL,
		/* DOM_NODE_NOTATION		*/ NULL,
	}
};

static enum dom_code
sgml_error_function(struct sgml_parser *parser, struct dom_string *string,
		    unsigned int line_number)
{
	printf("error on line %d: %.*s\n",
	       line_number, string->length, string->string);

	return DOM_CODE_OK;
}

int
main(int argc, char *argv[])
{
	struct sgml_parser *parser;
	enum sgml_document_type doctype = SGML_DOCTYPE_HTML;
	enum sgml_parser_flag flags = 0;
	enum sgml_parser_type type = SGML_PARSER_STREAM;
	enum dom_code code = 0;
	enum dom_config_flag normalize_flags = 0;
	struct dom_config config;
	int normalize = 0;
	int dump = 0;
	int complete = 1;
	size_t read_stdin = 0;
	struct dom_string uri = STATIC_DOM_STRING("dom://test");
	struct dom_string source = STATIC_DOM_STRING("(no source)");
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

		} else if (get_test_opt(&arg, "stdin", &i, argc, argv, "a number")) {
			read_stdin = atoi(arg);
			flags |= SGML_PARSER_INCREMENTAL;

		} else if (get_test_opt(&arg, "normalize", &i, argc, argv, "a string")) {
			normalize = 1;
			normalize_flags = parse_dom_config(arg, ',');
			type = SGML_PARSER_TREE;

		} else if (!strcmp(arg, "print-lines")) {
			flags |= SGML_PARSER_COUNT_LINES;

		} else if (!strcmp(arg, "incomplete")) {
			flags |= SGML_PARSER_INCREMENTAL;
			complete = 0;

		} else if (!strcmp(arg, "dump")) {
			type = SGML_PARSER_TREE;
			dump = 1;

		} else if (!strcmp(arg, "error")) {
			flags |= SGML_PARSER_DETECT_ERRORS;

		} else if (!strcmp(arg, "help")) {
			die(NULL);

		} else {
			die("Unknown argument '%s'", arg - 2);
		}
	}

	parser = init_sgml_parser(type, doctype, &uri, flags);
	if (!parser) return 1;

	parser->error_func = sgml_error_function;
	if (normalize)
		add_dom_config_normalizer(&parser->stack, &config, normalize_flags);
	else if (!dump)
		add_dom_stack_context(&parser->stack, NULL, &sgml_parser_test_context_info);

	if (read_stdin > 0) {
		unsigned char *buffer;

		buffer = mem_alloc(read_stdin);
		if (!buffer)
			die("Cannot allocate buffer");

		complete = 0;

		while (!complete) {
			size_t size = fread(buffer, 1, read_stdin, stdin);

			if (ferror(stdin))
				die("error reading from stdin");

			complete = feof(stdin);

			code = parse_sgml(parser, buffer, size, complete);
			switch (code) {
			case DOM_CODE_OK:
				break;

			case DOM_CODE_INCOMPLETE:
				if (!complete) break;
				/* Error */
			default:
				complete = 1;
			}
		}

		mem_free(buffer);

	} else {
		code = parse_sgml(parser, source.string, source.length, complete);
	}

	if (parser->root) {
		assert(!complete || parser->stack.depth > 0);

		while (!dom_stack_is_empty(&parser->stack)) {
			get_dom_stack_top(&parser->stack)->immutable = 0;
			pop_dom_node(&parser->stack);
		}

		if (normalize || dump) {
			struct dom_stack stack;

			/* Note, that we cannot free nodes when walking the DOM
			 * tree since walk_dom_node() uses an index to traverse
			 * the tree. */
			init_dom_stack(&stack, DOM_STACK_FLAG_NONE);
			/* XXX: This context needs to be added first because it
			 * assumes the parser can be accessed via
			 * stack->contexts[0].data. */
			if (normalize)
				add_dom_stack_context(&stack, parser, &sgml_parser_test_context_info);
			else if (dump)
				add_sgml_file_dumper(&stack, stdout);
			walk_dom_nodes(&stack, parser->root);
			done_dom_stack(&stack);
			done_dom_node(parser->root);
		}
	}

	done_sgml_parser(parser);
#ifdef DEBUG_MEMLEAK
	check_memory_leaks();
#endif

	return code != DOM_CODE_OK ? 1 : 0;
}
