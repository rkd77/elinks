/* SGML node handling */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "dom/node.h"
#include "dom/sgml/parser.h"
#include "dom/sgml/scanner.h"
#include "dom/sgml/sgml.h"
#include "dom/stack.h"
#include "dom/string.h"
#include "util/error.h"
#include "util/memory.h"


/* When getting the sgml_parser struct it is _always_ assumed that the parser
 * is the first to add it's context, which it is since it initializes the
 * stack. */

#define get_sgml_parser(stack) ((stack)->contexts[0]->data)

#define get_sgml_parser_state(stack, state) \
	get_dom_stack_state_data(stack->contexts[0], state)


/* Functions for adding new nodes to the DOM tree: */

/* They wrap init_dom_node() and add_dom_*() and set up of additional
 * information like node subtypes and SGML parser state information. */

static inline struct dom_node *
add_sgml_document(struct sgml_parser *parser)
{
	int allocated = parser->flags & SGML_PARSER_INCREMENTAL;
	struct dom_node *node;

	node = init_dom_node(DOM_NODE_DOCUMENT, &parser->uri, allocated);
	if (node && push_dom_node(&parser->stack, node) == DOM_CODE_OK)
		return node;

	return NULL;
}

static inline struct dom_node *
add_sgml_element(struct dom_stack *stack, struct dom_scanner_token *token)
{
	struct sgml_parser *parser = get_sgml_parser(stack);
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	struct dom_stack_state *state;
	struct sgml_parser_state *pstate;
	struct dom_node *node;
	struct sgml_node_info *node_info;

	node = add_dom_element(parent, &token->string);
	if (!node) return NULL;

	node_info = get_sgml_node_info(parser->info->elements, node);
	node->data.element.type = node_info->type;

	if (push_dom_node(stack, node) != DOM_CODE_OK)
		return NULL;

	state = get_dom_stack_top(stack);
	assert(node == state->node);

	pstate = get_sgml_parser_state(stack, state);
	pstate->info = node_info;

	return node;
}


static inline struct dom_node *
add_sgml_attribute(struct dom_stack *stack,
		   struct dom_scanner_token *token, struct dom_scanner_token *valtoken)
{
	struct sgml_parser *parser = get_sgml_parser(stack);
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	struct dom_string *value = valtoken ? &valtoken->string : NULL;
	struct sgml_node_info *info;
	struct dom_node *node;

	node = add_dom_attribute(parent, &token->string, value);

	info = get_sgml_node_info(parser->info->attributes, node);

	node->data.attribute.type      = info->type;
	node->data.attribute.id	       = !!(info->flags & SGML_ATTRIBUTE_IDENTIFIER);
	node->data.attribute.reference = !!(info->flags & SGML_ATTRIBUTE_REFERENCE);

	if (valtoken && valtoken->type == SGML_TOKEN_STRING)
		node->data.attribute.quoted = valtoken->string.string[-1];

	if (!node || push_dom_node(stack, node) != DOM_CODE_OK)
		return NULL;

	pop_dom_node(stack);

	return node;
}

static inline struct dom_node *
add_sgml_proc_instruction(struct dom_stack *stack, struct dom_scanner_token *target,
			  struct dom_scanner_token *data)
{
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	struct dom_string *data_str = data ? &data->string : NULL;
	struct dom_node *node;

	node = add_dom_proc_instruction(parent, &target->string, data_str);
	if (!node) return NULL;

	switch (target->type) {
	case SGML_TOKEN_PROCESS_XML:
		node->data.proc_instruction.type = DOM_PROC_INSTRUCTION_XML;
		break;

	case SGML_TOKEN_PROCESS_XML_STYLESHEET:
		node->data.proc_instruction.type = DOM_PROC_INSTRUCTION_XML_STYLESHEET;
		break;

	case SGML_TOKEN_PROCESS:
	default:
		node->data.proc_instruction.type = DOM_PROC_INSTRUCTION;
	}

	if (push_dom_node(stack, node) == DOM_CODE_OK)
		return node;

	return NULL;
}

static inline struct dom_node *
add_sgml_node(struct dom_stack *stack, enum dom_node_type type, struct dom_scanner_token *token)
{
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	struct dom_node *node = add_dom_node(parent, type, &token->string);

	if (!node) return NULL;

	if (token->type == SGML_TOKEN_SPACE)
		node->data.text.only_space = 1;

	if (push_dom_node(stack, node) == DOM_CODE_OK)
		pop_dom_node(stack);

	return node;
}


/* SGML parser main handling: */

static enum dom_code
call_sgml_error_function(struct dom_stack *stack, struct dom_scanner_token *token)
{
	struct sgml_parser *parser = get_sgml_parser(stack);
	unsigned int line = get_sgml_parser_line_number(parser);

	assert(parser->error_func);

	return parser->error_func(parser, &token->string, line);
}

/* Appends to or 'creates' an incomplete token. This can be used to
 * force tokens back into the 'stream' if they require that later tokens
 * are available.
 *
 * NOTE: You can only do this for tokens that are not stripped of markup such
 * as identifiers. */
static enum dom_code
check_sgml_incomplete(struct dom_scanner *scanner,
		      struct dom_scanner_token *start,
		      struct dom_scanner_token *token)
{
	if (token && token->type == SGML_TOKEN_INCOMPLETE) {
		token->string.length += token->string.string - start->string.string;
		token->string.string = start->string.string;
		return 1;

	} else if (!token && scanner->check_complete && scanner->incomplete) {
		size_t left = scanner->end - start->string.string;

		assert(left > 0);

		token = scanner->current = scanner->table;
		scanner->tokens = 1;
		token->type = SGML_TOKEN_INCOMPLETE;
		set_dom_string(&token->string, start->string.string, left);
		return 1;
	}

	return 0;
}

static inline enum dom_code
parse_sgml_attributes(struct dom_stack *stack, struct dom_scanner *scanner)
{
	struct dom_scanner_token name;

	while (dom_scanner_has_tokens(scanner)) {
		struct dom_scanner_token *token = get_dom_scanner_token(scanner);

		assert(token);

		switch (token->type) {
		case SGML_TOKEN_TAG_END:
			skip_dom_scanner_token(scanner);
			/* and return */
		case SGML_TOKEN_ELEMENT:
		case SGML_TOKEN_ELEMENT_BEGIN:
		case SGML_TOKEN_ELEMENT_END:
		case SGML_TOKEN_ELEMENT_EMPTY_END:
			return DOM_CODE_OK;

		case SGML_TOKEN_IDENT:
			copy_struct(&name, token);

			/* Skip the attribute name token */
			token = get_next_dom_scanner_token(scanner);

			if (token && token->type == '=') {
				/* If the token is not a valid value token
				 * ignore it. */
				token = get_next_dom_scanner_token(scanner);
				if (check_sgml_incomplete(scanner, &name, token))
					return DOM_CODE_INCOMPLETE;

				if (token
				    && token->type != SGML_TOKEN_IDENT
				    && token->type != SGML_TOKEN_ATTRIBUTE
				    && token->type != SGML_TOKEN_STRING)
					token = NULL;

			} else if (check_sgml_incomplete(scanner, &name, token)) {
				return DOM_CODE_INCOMPLETE;

			} else {
				token = NULL;
			}

			if (!add_sgml_attribute(stack, &name, token))
				return DOM_CODE_ALLOC_ERR;

			/* Skip the value token */
			if (token)
				skip_dom_scanner_token(scanner);
			break;

		case SGML_TOKEN_INCOMPLETE:
			return DOM_CODE_INCOMPLETE;

		case SGML_TOKEN_ERROR:
		{
			enum dom_code code;

			code = call_sgml_error_function(stack, token);
			if (code != DOM_CODE_OK)
				return code;

			skip_dom_scanner_token(scanner);
			break;
		}
		default:
			skip_dom_scanner_token(scanner);
		}
	}

	return DOM_CODE_OK;
}

static enum dom_code
parse_sgml_plain(struct dom_stack *stack, struct dom_scanner *scanner)
{
	struct dom_scanner_token target;

	while (dom_scanner_has_tokens(scanner)) {
		struct dom_scanner_token *token = get_dom_scanner_token(scanner);

		switch (token->type) {
		case SGML_TOKEN_ELEMENT:
		case SGML_TOKEN_ELEMENT_BEGIN:
			if (!add_sgml_element(stack, token))
				return DOM_CODE_ALLOC_ERR;

			if (token->type == SGML_TOKEN_ELEMENT_BEGIN) {
				enum dom_code code;

				skip_dom_scanner_token(scanner);

				code = parse_sgml_attributes(stack, scanner);
				if (code != DOM_CODE_OK)
					return code;

			} else {
				skip_dom_scanner_token(scanner);
			}

			break;

		case SGML_TOKEN_ELEMENT_EMPTY_END:
			pop_dom_node(stack);
			skip_dom_scanner_token(scanner);
			break;

		case SGML_TOKEN_ELEMENT_END:
			if (!token->string.length) {
				pop_dom_node(stack);
			} else {
				struct dom_string string;
				struct dom_stack_state *state;

				set_dom_string(&string, token->string.string, token->string.length);
				state = search_dom_stack(stack, DOM_NODE_ELEMENT,
							 &string);
				if (state) {
					struct sgml_parser_state *pstate;

					pstate = get_sgml_parser_state(stack, state);
					copy_struct(&pstate->end_token, token);

					pop_dom_state(stack, state);
				}
			}
			skip_dom_scanner_token(scanner);
			break;

		case SGML_TOKEN_NOTATION_COMMENT:
			if (!add_sgml_node(stack, DOM_NODE_COMMENT, token))
				return DOM_CODE_ALLOC_ERR;
			skip_dom_scanner_token(scanner);
			break;

		case SGML_TOKEN_NOTATION_ATTLIST:
		case SGML_TOKEN_NOTATION_DOCTYPE:
		case SGML_TOKEN_NOTATION_ELEMENT:
		case SGML_TOKEN_NOTATION_ENTITY:
		case SGML_TOKEN_NOTATION:
			skip_dom_scanner_token(scanner);
			break;

		case SGML_TOKEN_CDATA_SECTION:
			if (!add_sgml_node(stack, DOM_NODE_CDATA_SECTION, token))
				return DOM_CODE_ALLOC_ERR;
			skip_dom_scanner_token(scanner);
			break;

		case SGML_TOKEN_PROCESS_XML_STYLESHEET:
		case SGML_TOKEN_PROCESS_XML:
		case SGML_TOKEN_PROCESS:
			copy_struct(&target, token);

			/* Skip the target token */
			token = get_next_dom_scanner_token(scanner);
			if (!token || token->type == SGML_TOKEN_INCOMPLETE)
				return DOM_CODE_INCOMPLETE;

			if (token->type == SGML_TOKEN_ERROR)
				break;

			assert(token->type == SGML_TOKEN_PROCESS_DATA);
			/* Fall-through */

			if (!add_sgml_proc_instruction(stack, &target, token))
				return DOM_CODE_ALLOC_ERR;
			if ((target.type == SGML_TOKEN_PROCESS_XML
			     || target.type == SGML_TOKEN_PROCESS_XML_STYLESHEET)
			    && token->string.length > 0) {
				/* Parse the <?xml data="attributes"?>. */
				struct dom_scanner attr_scanner;

				/* The attribute souce is complete. */
				init_dom_scanner(&attr_scanner, &sgml_scanner_info,
						 &token->string, SGML_STATE_ELEMENT,
						 scanner->count_lines, 1, 0, 0);

				if (dom_scanner_has_tokens(&attr_scanner)) {
					/* Ignore parser codes from this
					 * enhanced parsing of attributes. It
					 * is really just a simple way to try
					 * and support xml and xml-stylesheet
					 * instructions. */
					parse_sgml_attributes(stack, &attr_scanner);
				}
			}

			pop_dom_node(stack);
			skip_dom_scanner_token(scanner);
			break;

		case SGML_TOKEN_ENTITY:
			add_sgml_node(stack, DOM_NODE_ENTITY_REFERENCE, token);
			skip_dom_scanner_token(scanner);
			break;

		case SGML_TOKEN_INCOMPLETE:
			return DOM_CODE_INCOMPLETE;

		case SGML_TOKEN_ERROR:
		{
			enum dom_code code;

			code = call_sgml_error_function(stack, token);
			if (code != DOM_CODE_OK)
				return code;

			skip_dom_scanner_token(scanner);
			break;
		}
		case SGML_TOKEN_SPACE:
		case SGML_TOKEN_TEXT:
		default:
			add_sgml_node(stack, DOM_NODE_TEXT, token);
			skip_dom_scanner_token(scanner);
		}
	}

	return DOM_CODE_OK;
}

enum dom_code
parse_sgml(struct sgml_parser *parser, unsigned char *buf, size_t bufsize,
	   int complete)
{
	struct dom_string source = INIT_DOM_STRING(buf, bufsize);
	struct dom_node *node;

	if (complete)
		parser->flags |= SGML_PARSER_COMPLETE;

	if (!parser->root) {
		parser->root = add_sgml_document(parser);
		if (!parser->root)
			return DOM_CODE_ALLOC_ERR;
		get_dom_stack_top(&parser->stack)->immutable = 1;
	}

	node = init_dom_node(DOM_NODE_TEXT, &source, 0);
	if (!node || push_dom_node(&parser->parsing, node) != DOM_CODE_OK)
		return DOM_CODE_ALLOC_ERR;

	return parser->code;
}


/* Parsing state management: */

/* The SGML parser can handle nested calls to parse_sgml(). This can be used to
 * handle output of external processing of data in the document tree. For
 * example this can allows output of the document.write() from DOM scripting
 * interface to be parsed. */

/* This holds info about a chunk of text being parsed. */
struct sgml_parsing_state {
	struct dom_scanner scanner;
	struct dom_node *node;
	struct dom_string incomplete;
	size_t depth;
	unsigned int resume:1;
};

enum dom_code
sgml_parsing_push(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct sgml_parser *parser = get_sgml_parser(stack);
	struct sgml_parsing_state *parsing = data;
	int count_lines = !!(parser->flags & SGML_PARSER_COUNT_LINES);
	int complete = !!(parser->flags & SGML_PARSER_COMPLETE);
	int incremental = !!(parser->flags & SGML_PARSER_INCREMENTAL);
	int detect_errors = !!(parser->flags & SGML_PARSER_DETECT_ERRORS);
	struct dom_string *string = &node->string;
	struct dom_scanner_token *token;
	struct dom_string incomplete;
	enum sgml_scanner_state scanner_state = SGML_STATE_TEXT;

	parsing->depth = parser->stack.depth;

	if (stack->depth > 1) {
		struct sgml_parsing_state *parent = &parsing[-1];

		if (parent->resume) {
			if (is_dom_string_set(&parent->incomplete)) {

				if (!add_to_dom_string(&parent->incomplete,
						       string->string,
						       string->length)) {

					parser->code = DOM_CODE_ALLOC_ERR;
					return DOM_CODE_OK;
				}

				string = &parent->incomplete;
			}

			scanner_state = parent->scanner.state;

			/* Pop down to the parent. */
			parsing = parent;
			parsing->resume = 0;
			pop_dom_node(stack);
		}
	}

	init_dom_scanner(&parsing->scanner, &sgml_scanner_info, string,
			 scanner_state, count_lines, complete, incremental,
			 detect_errors);

	if (scanner_state == SGML_STATE_ELEMENT) {
		parser->code = parse_sgml_attributes(&parser->stack, &parsing->scanner);
		if (parser->code == DOM_CODE_OK)
			parser->code = parse_sgml_plain(&parser->stack, &parsing->scanner);
	} else {
		parser->code = parse_sgml_plain(&parser->stack, &parsing->scanner);
	}

	if (complete) {
		pop_dom_node(&parser->parsing);
		return DOM_CODE_OK;
	}

	if (parser->code != DOM_CODE_INCOMPLETE) {
		/* No need to preserve the default scanner state. */
		if (parsing->scanner.state == SGML_STATE_TEXT) {
			pop_dom_node(&parser->parsing);
			return DOM_CODE_OK;
		}

		done_dom_string(&parsing->incomplete);
		parsing->resume = 1;
		return DOM_CODE_OK;
	}

	token = get_dom_scanner_token(&parsing->scanner);
	assert(token && token->type == SGML_TOKEN_INCOMPLETE);

	string = &token->string;

	set_dom_string(&incomplete, NULL, 0);

	if (!init_dom_string(&incomplete, string->string, string->length)) {
		parser->code = DOM_CODE_ALLOC_ERR;
		return DOM_CODE_OK;
	}

	done_dom_string(&parsing->incomplete);
	set_dom_string(&parsing->incomplete, incomplete.string, incomplete.length);
	parsing->resume = 1;

	return DOM_CODE_OK;
}

enum dom_code
sgml_parsing_pop(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct sgml_parser *parser = get_sgml_parser(stack);
	struct sgml_parsing_state *parsing = data;

	/* Only clean up the stack if complete so that we get proper nesting. */
	if (parser->flags & SGML_PARSER_COMPLETE) {
		/* Pop the stack back to the state it was in. This includes cleaning
		 * away even immutable states left on the stack. */
		while (parsing->depth < parser->stack.depth) {
			get_dom_stack_top(&parser->stack)->immutable = 0;
			pop_dom_node(&parser->stack);
		}
		/* It's bigger than when calling done_sgml_parser() in the middle of an
		 * incomplete parsing. */
		assert(parsing->depth >= parser->stack.depth);
	}

	done_dom_string(&parsing->incomplete);

	return DOM_CODE_OK;
}

static struct dom_stack_context_info sgml_parsing_context_info = {
	/* Object size: */			sizeof(struct sgml_parsing_state),
	/* Push: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ NULL,
		/* DOM_NODE_ATTRIBUTE		*/ NULL,
		/* DOM_NODE_TEXT		*/ sgml_parsing_push,
		/* DOM_NODE_CDATA_SECTION	*/ NULL,
		/* DOM_NODE_ENTITY_REFERENCE	*/ NULL,
		/* DOM_NODE_ENTITY		*/ NULL,
		/* DOM_NODE_PROC_INSTRUCTION	*/ NULL,
		/* DOM_NODE_COMMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT_TYPE	*/ NULL,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ NULL,
		/* DOM_NODE_NOTATION		*/ NULL,
	},
	/* Pop: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ NULL,
		/* DOM_NODE_ATTRIBUTE		*/ NULL,
		/* DOM_NODE_TEXT		*/ sgml_parsing_pop,
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

unsigned int
get_sgml_parser_line_number(struct sgml_parser *parser)
{
	struct dom_stack_state *state;
	struct sgml_parsing_state *pstate;

	assert(parser->flags & SGML_PARSER_COUNT_LINES);

	if (dom_stack_is_empty(&parser->parsing))
		return 0;

	state = get_dom_stack_top(&parser->parsing);
	pstate = get_dom_stack_state_data(parser->parsing.contexts[0], state);

	assert(pstate->scanner.count_lines && pstate->scanner.lineno);

	if (pstate->scanner.current
	    && pstate->scanner.current < pstate->scanner.table + DOM_SCANNER_TOKENS
	    && pstate->scanner.current->type == SGML_TOKEN_ERROR)
		return pstate->scanner.current->lineno;

	return pstate->scanner.lineno;
}


/* Parser creation and destruction: */

/* FIXME: For now the main SGML parser context doesn't do much other than
 * declaring the sgml_parser_state object. */
static struct dom_stack_context_info sgml_parser_context_info = {
	/* Object size: */			sizeof(struct sgml_parser_state),
	/* Push: */
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

struct sgml_parser *
init_sgml_parser(enum sgml_parser_type type, enum sgml_document_type doctype,
		 struct dom_string *uri, enum sgml_parser_flag flags)
{
	struct sgml_parser *parser;
	enum dom_stack_flag stack_flags = 0;

	parser = mem_calloc(1, sizeof(*parser));
	if (!parser) return NULL;

	if (!init_dom_string(&parser->uri, uri->string, uri->length)) {
		mem_free(parser);
		return NULL;
	}

	if (flags & SGML_PARSER_DETECT_ERRORS)
		flags |= SGML_PARSER_COUNT_LINES;

	parser->type  = type;
	parser->flags = flags;
	parser->info  = get_sgml_info(doctype);

	if (type == SGML_PARSER_STREAM)
		stack_flags |= DOM_STACK_FLAG_FREE_NODES;

	init_dom_stack(&parser->stack, stack_flags);
	/* FIXME: Some sgml backend specific callbacks? Handle HTML script tags,
	 * and feed document.write() data back to the parser. */
	add_dom_stack_context(&parser->stack, parser, &sgml_parser_context_info);

	/* Don't keep the 'fake' text nodes that holds the parsing data. */
	init_dom_stack(&parser->parsing, DOM_STACK_FLAG_FREE_NODES);
	add_dom_stack_context(&parser->parsing, parser, &sgml_parsing_context_info);

	return parser;
}

void
done_sgml_parser(struct sgml_parser *parser)
{
	while (!dom_stack_is_empty(&parser->parsing))
		pop_dom_node(&parser->parsing);
	done_dom_stack(&parser->parsing);
	done_dom_stack(&parser->stack);
	done_dom_string(&parser->uri);
	mem_free(parser);
}
