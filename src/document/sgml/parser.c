/* SGML node handling */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "document/document.h"
#include "document/dom/node.h"
#include "document/dom/stack.h"
#include "document/html/renderer.h" /* TODO: Move get_convert_table() */
#include "document/sgml/html/html.h"
#include "document/sgml/parser.h"
#include "document/sgml/scanner.h"
#include "document/sgml/sgml.h"
#include "intl/charsets.h"
#include "protocol/uri.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


/* Functions for adding new nodes to the DOM tree */

static inline struct dom_node *
add_sgml_document(struct dom_stack *stack, struct uri *uri)
{
	unsigned char *string = struri(uri);
	int length = strlen(string);
	struct dom_node *node = init_dom_node(DOM_NODE_DOCUMENT, string, length);

	return node ? push_dom_node(stack, node) : node;
}

static inline struct dom_node *
add_sgml_element(struct dom_stack *stack, struct scanner_token *token)
{
	struct sgml_parser *parser = stack->data;
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	struct dom_stack_state *state;
	struct sgml_parser_state *pstate;
	struct dom_node *node;

	node = add_dom_element(parent, token->string, token->length);

	if (!node || !push_dom_node(stack, node))
		return NULL;

	state = get_dom_stack_top(stack);
	assert(node == state->node && state->data);

	pstate = state->data;
	pstate->info = get_sgml_node_info(parser->info->elements, node);
	node->data.element.type = pstate->info->type;

	return node;
}


static inline void
add_sgml_attribute(struct dom_stack *stack,
		  struct scanner_token *token, struct scanner_token *valtoken)
{
	struct sgml_parser *parser = stack->data;
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	unsigned char *value = valtoken ? valtoken->string : NULL;
	uint16_t valuelen = valtoken ? valtoken->length : 0;
	struct sgml_node_info *info;
	struct dom_node *node;

	node = add_dom_attribute(parent, token->string, token->length,
				 value, valuelen);

	if (!node || !push_dom_node(stack, node))
		return;

	info = get_sgml_node_info(parser->info->attributes, node);

	node->data.attribute.type      = info->type;
	node->data.attribute.id	       = !!(info->flags & SGML_ATTRIBUTE_IDENTIFIER);
	node->data.attribute.reference = !!(info->flags & SGML_ATTRIBUTE_REFERENCE);

	if (valtoken && valtoken->type == SGML_TOKEN_STRING)
		node->data.attribute.quoted = 1;

	pop_dom_node(stack);
}

static inline struct dom_node *
add_sgml_proc_instruction(struct dom_stack *stack, struct scanner_token *token)
{
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	struct dom_node *node;
	/* Split the token in two if we can find a first space separator. */
	unsigned char *separator = memchr(token->string, ' ', token->length);

	/* Anything before the separator becomes the target name ... */
	unsigned char *name = token->string;
	int namelen = separator ? separator - token->string : token->length;

	/* ... and everything after the instruction value. */
	unsigned char *value = separator ? separator + 1 : NULL;
	int valuelen = value ? token->length - namelen - 1 : 0;

	node = add_dom_proc_instruction(parent, name, namelen, value, valuelen);
	if (!node) return NULL;

	switch (token->type) {
	case SGML_TOKEN_PROCESS_XML:
		node->data.proc_instruction.type = DOM_PROC_INSTRUCTION_XML;
		break;

	case SGML_TOKEN_PROCESS:
	default:
		node->data.proc_instruction.type = DOM_PROC_INSTRUCTION;
	}

	if (!push_dom_node(stack, node))
		return NULL;

	if (token->type != SGML_TOKEN_PROCESS_XML)
		pop_dom_node(stack);

	return node;
}

static inline void
add_sgml_node(struct dom_stack *stack, enum dom_node_type type, struct scanner_token *token)
{
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	struct dom_node *node = add_dom_node(parent, type, token->string, token->length);

	if (!node) return;

	if (token->type == SGML_TOKEN_SPACE)
		node->data.text.only_space = 1;

	if (push_dom_node(stack, node))
		pop_dom_node(stack);
}

#define add_sgml_entityref(stack, t)	add_sgml_node(stack, DOM_NODE_ENTITY_REFERENCE, t)
#define add_sgml_text(stack, t)		add_sgml_node(stack, DOM_NODE_TEXT, t)
#define add_sgml_comment(stack, t)	add_sgml_node(stack, DOM_NODE_COMMENT, t)

static inline void
parse_sgml_attributes(struct dom_stack *stack, struct scanner *scanner)
{
	struct scanner_token name;

	assert(scanner_has_tokens(scanner)
	       && (get_scanner_token(scanner)->type == SGML_TOKEN_ELEMENT_BEGIN
	       	   || get_scanner_token(scanner)->type == SGML_TOKEN_PROCESS_XML));

	skip_scanner_token(scanner);

	while (scanner_has_tokens(scanner)) {
		struct scanner_token *token = get_scanner_token(scanner);

		assert(token);

		switch (token->type) {
		case SGML_TOKEN_TAG_END:
			skip_scanner_token(scanner);
			/* and return */
		case SGML_TOKEN_ELEMENT:
		case SGML_TOKEN_ELEMENT_BEGIN:
		case SGML_TOKEN_ELEMENT_END:
		case SGML_TOKEN_ELEMENT_EMPTY_END:
			return;

		case SGML_TOKEN_IDENT:
			copy_struct(&name, token);

			/* Skip the attribute name token */
			token = get_next_scanner_token(scanner);
			if (token && token->type == '=') {
				/* If the token is not a valid value token
				 * ignore it. */
				token = get_next_scanner_token(scanner);
				if (token
				    && token->type != SGML_TOKEN_IDENT
				    && token->type != SGML_TOKEN_ATTRIBUTE
				    && token->type != SGML_TOKEN_STRING)
					token = NULL;
			} else {
				token = NULL;
			}

			add_sgml_attribute(stack, &name, token);

			/* Skip the value token */
			if (token)
				skip_scanner_token(scanner);
			break;

		default:
			skip_scanner_token(scanner);

		}
	}
}

void
parse_sgml_document(struct dom_stack *stack, struct scanner *scanner)
{
	while (scanner_has_tokens(scanner)) {
		struct scanner_token *token = get_scanner_token(scanner);

		switch (token->type) {
		case SGML_TOKEN_ELEMENT:
		case SGML_TOKEN_ELEMENT_BEGIN:
			if (!add_sgml_element(stack, token)) {
				if (token->type == SGML_TOKEN_ELEMENT) {
					skip_scanner_token(scanner);
					break;
				}

				skip_sgml_tokens(scanner, SGML_TOKEN_TAG_END);
				break;
			}

			if (token->type == SGML_TOKEN_ELEMENT_BEGIN) {
				parse_sgml_attributes(stack, scanner);
			} else {
				skip_scanner_token(scanner);
			}

			break;

		case SGML_TOKEN_ELEMENT_EMPTY_END:
			pop_dom_node(stack);
			skip_scanner_token(scanner);
			break;

		case SGML_TOKEN_ELEMENT_END:
			if (!token->length) {
				pop_dom_node(stack);
			} else {
				pop_dom_nodes(stack, DOM_NODE_ELEMENT,
					      token->string, token->length);
			}
			skip_scanner_token(scanner);
			break;

		case SGML_TOKEN_NOTATION_COMMENT:
			add_sgml_comment(stack, token);
			skip_scanner_token(scanner);
			break;

		case SGML_TOKEN_NOTATION_ATTLIST:
		case SGML_TOKEN_NOTATION_DOCTYPE:
		case SGML_TOKEN_NOTATION_ELEMENT:
		case SGML_TOKEN_NOTATION_ENTITY:
		case SGML_TOKEN_NOTATION:
			skip_scanner_token(scanner);
			break;

		case SGML_TOKEN_PROCESS_XML:
			if (!add_sgml_proc_instruction(stack, token)) {
				skip_sgml_tokens(scanner, SGML_TOKEN_TAG_END);
				break;
			}

			parse_sgml_attributes(stack, scanner);
			pop_dom_node(stack);
			break;

		case SGML_TOKEN_PROCESS:
			add_sgml_proc_instruction(stack, token);
			skip_scanner_token(scanner);
			break;

		case SGML_TOKEN_ENTITY:
			add_sgml_entityref(stack, token);
			skip_scanner_token(scanner);
			break;

		case SGML_TOKEN_SPACE:
		case SGML_TOKEN_TEXT:
		default:
			add_sgml_text(stack, token);
			skip_scanner_token(scanner);
		}
	}
}


static inline void
init_sgml_parser(struct sgml_parser *parser, struct document *document,
		 struct cache_entry *cache_entry, struct sgml_info *info,
		 struct string *buffer)
{
	unsigned char *source = buffer->source;
	unsigned char *end = source + buffer->length;

	memset(parser, 0, sizeof(*parser));

	init_scanner(&parser->scanner, &sgml_scanner_info, source, end);

	parser->document    = document;
	parser->cache_entry = cache_entry;
	parser->info	    = info;

	if (document->options.plain)
		parser->flags |= SGML_PARSER_ADD_ELEMENT_ENDS;
}

struct dom_node *
parse_sgml(struct cache_entry *cached, struct document *document,
	   struct string *buffer)
{
	struct dom_stack stack;
	struct sgml_parser parser;
	size_t obj_size = sizeof(struct sgml_parser_state);

	init_sgml_parser(&parser, document, cached, &sgml_html_info, buffer);
	init_dom_stack(&stack, &parser, parser.info->callbacks, obj_size);

	parser.root = add_sgml_document(&stack, document->uri);
	if (parser.root) {
		parse_sgml_document(&stack, &parser.scanner);
	}

	done_dom_stack(&stack);

	return parser.root;
}
