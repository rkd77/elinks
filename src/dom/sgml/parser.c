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


/* This holds info about a chunk of text being parsed. The SGML parser uses
 * these to keep track of possible nested calls to parse_sgml(). This can be
 * used to feed output of stuff like ECMAScripts document.write() from
 * <script>-elements back to the SGML parser. */
struct sgml_parsing_state {
	struct dom_scanner scanner;
	struct dom_node *node;
	size_t depth;
};

static struct sgml_parsing_state *
init_sgml_parsing_state(struct sgml_parser *parser, struct dom_string *buffer);


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
add_sgml_document(struct dom_stack *stack, struct dom_string *string)
{
	struct dom_node *node = init_dom_node(DOM_NODE_DOCUMENT, string);

	return node ? push_dom_node(stack, node) : NULL;
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

	if (!push_dom_node(stack, node))
		return NULL;

	state = get_dom_stack_top(stack);
	assert(node == state->node);

	pstate = get_sgml_parser_state(stack, state);
	pstate->info = node_info;

	return node;
}


static inline void
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
		node->data.attribute.quoted = 1;

	if (!node || !push_dom_node(stack, node))
		return;

	pop_dom_node(stack);
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

	case SGML_TOKEN_PROCESS:
	default:
		node->data.proc_instruction.type = DOM_PROC_INSTRUCTION;
	}

	return push_dom_node(stack, node);
}

static inline void
add_sgml_node(struct dom_stack *stack, enum dom_node_type type, struct dom_scanner_token *token)
{
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	struct dom_node *node = add_dom_node(parent, type, &token->string);

	if (!node) return;

	if (token->type == SGML_TOKEN_SPACE)
		node->data.text.only_space = 1;

	if (push_dom_node(stack, node))
		pop_dom_node(stack);
}


/* SGML parser main handling: */

static inline enum sgml_parser_code
parse_sgml_attributes(struct dom_stack *stack, struct dom_scanner *scanner)
{
	struct dom_scanner_token name;

	assert(dom_scanner_has_tokens(scanner)
	       && (get_dom_scanner_token(scanner)->type == SGML_TOKEN_ELEMENT_BEGIN
	           || (get_dom_stack_top(stack)->node->type == DOM_NODE_PROCESSING_INSTRUCTION)));

	if (get_dom_scanner_token(scanner)->type == SGML_TOKEN_ELEMENT_BEGIN)
		skip_dom_scanner_token(scanner);

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
			return SGML_PARSER_CODE_OK;

		case SGML_TOKEN_IDENT:
			copy_struct(&name, token);

			/* Skip the attribute name token */
			token = get_next_dom_scanner_token(scanner);

			if (token && token->type == '=') {
				/* If the token is not a valid value token
				 * ignore it. */
				token = get_next_dom_scanner_token(scanner);
				if (token && token->type == SGML_TOKEN_INCOMPLETE)
					return SGML_PARSER_CODE_INCOMPLETE;

				if (token
				    && token->type != SGML_TOKEN_IDENT
				    && token->type != SGML_TOKEN_ATTRIBUTE
				    && token->type != SGML_TOKEN_STRING)
					token = NULL;

			} else if (token && token->type == SGML_TOKEN_INCOMPLETE) {
				return SGML_PARSER_CODE_INCOMPLETE;

			} else {
				token = NULL;
			}

			add_sgml_attribute(stack, &name, token);

			/* Skip the value token */
			if (token)
				skip_dom_scanner_token(scanner);
			break;

		case SGML_TOKEN_INCOMPLETE:
			return SGML_PARSER_CODE_INCOMPLETE;

		default:
			skip_dom_scanner_token(scanner);
		}
	}

	return SGML_PARSER_CODE_OK;
}

static enum sgml_parser_code
parse_sgml_plain(struct dom_stack *stack, struct dom_scanner *scanner)
{
	struct dom_scanner_token target;

	while (dom_scanner_has_tokens(scanner)) {
		struct dom_scanner_token *token = get_dom_scanner_token(scanner);

		switch (token->type) {
		case SGML_TOKEN_ELEMENT:
		case SGML_TOKEN_ELEMENT_BEGIN:
			if (!add_sgml_element(stack, token)) {
				if (token->type == SGML_TOKEN_ELEMENT) {
					skip_dom_scanner_token(scanner);
					break;
				}

				skip_sgml_tokens(scanner, SGML_TOKEN_TAG_END);
				break;
			}

			if (token->type == SGML_TOKEN_ELEMENT_BEGIN) {
				enum sgml_parser_code code;

				code = parse_sgml_attributes(stack, scanner);
				if (code != SGML_PARSER_CODE_OK)
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
			add_sgml_node(stack, DOM_NODE_COMMENT, token);
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
			add_sgml_node(stack, DOM_NODE_CDATA_SECTION, token);
			skip_dom_scanner_token(scanner);
			break;

		case SGML_TOKEN_PROCESS_XML_STYLESHEET:
		case SGML_TOKEN_PROCESS_XML:
		case SGML_TOKEN_PROCESS:
			copy_struct(&target, token);

			/* Skip the target token */
			token = get_next_dom_scanner_token(scanner);
			if (!token || token->type == SGML_TOKEN_INCOMPLETE)
				return SGML_PARSER_CODE_INCOMPLETE;

			assert(token->type == SGML_TOKEN_PROCESS_DATA);

			if (add_sgml_proc_instruction(stack, &target, token)
			    && (target.type == SGML_TOKEN_PROCESS_XML
			        || target.type == SGML_TOKEN_PROCESS_XML_STYLESHEET)
			    && token->string.length > 0) {
				/* Parse the <?xml data="attributes"?>. */
				struct dom_scanner attr_scanner;

				/* The attribute souce is complete. */
				init_dom_scanner(&attr_scanner, &sgml_scanner_info,
						 &token->string, SGML_STATE_ELEMENT,
						 scanner->count_lines, 1, 0);

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
			return SGML_PARSER_CODE_INCOMPLETE;

		case SGML_TOKEN_SPACE:
		case SGML_TOKEN_TEXT:
		default:
			add_sgml_node(stack, DOM_NODE_TEXT, token);
			skip_dom_scanner_token(scanner);
		}
	}

	return SGML_PARSER_CODE_OK;
}

enum sgml_parser_code
parse_sgml(struct sgml_parser *parser, struct dom_string *buffer, int complete)
{
	struct sgml_parsing_state *parsing;
	enum sgml_parser_code code;

	if (complete)
		parser->flags |= SGML_PARSER_COMPLETE;

	if (!parser->root) {
		parser->root = add_sgml_document(&parser->stack, &parser->uri);
		if (!parser->root)
			return SGML_PARSER_CODE_MEM_ALLOC;
		get_dom_stack_top(&parser->stack)->immutable = 1;
	}

	parsing = init_sgml_parsing_state(parser, buffer);
	if (!parsing) return SGML_PARSER_CODE_MEM_ALLOC;

	code = parse_sgml_plain(&parser->stack, &parsing->scanner);

	pop_dom_node(&parser->parsing);

	return code;
}


/* Parsing state management: */

/* The SGML parser can handle nested calls to parse_sgml(). This can be used to
 * handle output of external processing of data in the document tree. For
 * example this can allows output of the document.write() from DOM scripting
 * interface to be parsed. */

static void
sgml_parsing_push(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct sgml_parser *parser = get_sgml_parser(stack);
	struct sgml_parsing_state *parsing = data;
	int count_lines = !!(parser->flags & SGML_PARSER_COUNT_LINES);
	int complete = !!(parser->flags & SGML_PARSER_COMPLETE);
	int incremental = !!(parser->flags & SGML_PARSER_INCREMENTAL);

	parsing->depth = parser->stack.depth;
	get_dom_stack_top(&parser->stack)->immutable = 1;
	init_dom_scanner(&parsing->scanner, &sgml_scanner_info, &node->string,
			 SGML_STATE_TEXT, count_lines, complete, incremental);
}

static void
sgml_parsing_pop(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct sgml_parser *parser = get_sgml_parser(stack);
	struct sgml_parsing_state *parsing = data;

	/* Pop the stack back to the state it was in. This includes cleaning
	 * away even immutable states left on the stack. */
	while (parsing->depth < parser->stack.depth) {
		get_dom_stack_top(&parser->stack)->immutable = 0;
		pop_dom_node(&parser->stack);
	}

	assert(parsing->depth == parser->stack.depth);
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

/* Create a new parsing state by pushing a new text node containing the*/
static struct sgml_parsing_state *
init_sgml_parsing_state(struct sgml_parser *parser, struct dom_string *buffer)
{
	struct dom_stack_state *state;
	struct dom_node *node;

	node = init_dom_node(DOM_NODE_TEXT, buffer);
	if (!node || !push_dom_node(&parser->parsing, node))
		return NULL;

	state = get_dom_stack_top(&parser->parsing);

	return get_dom_stack_state_data(parser->parsing.contexts[0], state);
}

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
	done_dom_stack(&parser->stack);
	done_dom_stack(&parser->parsing);
	done_dom_string(&parser->uri);
	mem_free(parser);
}
