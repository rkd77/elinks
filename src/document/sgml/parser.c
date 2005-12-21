/* SGML node handling */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/dom/node.h"
#include "document/dom/stack.h"
#include "document/sgml/parser.h"
#include "document/sgml/scanner.h"
#include "document/sgml/sgml.h"
#include "protocol/uri.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


#define get_sgml_parser(stack) ((stack)->data)

#define get_sgml_parser_state(stack, state) \
	get_dom_stack_state_data(stack, state)

/* Functions for adding new nodes to the DOM tree */

static inline struct dom_node *
add_sgml_document(struct dom_stack *stack, struct uri *uri)
{
	unsigned char *string = struri(uri);
	size_t length = strlen(string);
	struct dom_node *node = init_dom_node(DOM_NODE_DOCUMENT, string, length);

	return node ? push_dom_node(stack, node) : NULL;
}

static inline struct dom_node *
add_sgml_element(struct dom_stack *stack, struct scanner_token *token)
{
	struct sgml_parser *parser = get_sgml_parser(stack);
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	struct dom_stack_state *state;
	struct sgml_parser_state *pstate;
	struct dom_node *node;
	struct sgml_node_info *node_info;

	node = add_dom_element(parent, token->string, token->length);
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
		  struct scanner_token *token, struct scanner_token *valtoken)
{
	struct sgml_parser *parser = get_sgml_parser(stack);
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	unsigned char *value = valtoken ? valtoken->string : NULL;
	size_t valuelen = valtoken ? valtoken->length : 0;
	struct sgml_node_info *info;
	struct dom_node *node;

	node = add_dom_attribute(parent, token->string, token->length,
				 value, valuelen);

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
add_sgml_proc_instruction(struct dom_stack *stack, struct scanner_token *token)
{
	struct dom_node *parent = get_dom_stack_top(stack)->node;
	struct dom_node *node;
	/* Split the token in two if we can find a first space separator. */
	unsigned char *separator = memchr(token->string, ' ', token->length);

	/* Anything before the separator becomes the target name ... */
	unsigned char *name = token->string;
	size_t namelen = separator ? separator - token->string : token->length;

	/* ... and everything after the instruction value. */
	unsigned char *value = separator ? separator + 1 : NULL;
	size_t valuelen = value ? token->length - namelen - 1 : 0;

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
				struct dom_string string;
				struct dom_stack_state *state;

				set_dom_string(&string, token->string, token->length);
				state = search_dom_stack(stack, DOM_NODE_ELEMENT,
							 &string);
				if (state) {
					struct sgml_parser_state *pstate;

					pstate = get_sgml_parser_state(stack, state);
					copy_struct(&pstate->end_token, token);

					pop_dom_state(stack, state);
				}
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


struct sgml_parser *
init_sgml_parser(enum sgml_parser_type type, enum sgml_document_type doctype,
		 struct uri *uri)
{
	size_t obj_size = sizeof(struct sgml_parser_state);
	struct sgml_parser *parser;

	parser = mem_calloc(1, sizeof(*parser));
	if (!parser) return NULL;

	parser->type = type;
	parser->uri  = get_uri_reference(uri);
	parser->info = get_sgml_info(doctype);

	init_dom_stack(&parser->stack, parser, obj_size,
		       type != SGML_PARSER_STREAM);
	/* FIXME: Some sgml backend specific callbacks? Handle HTML script tags,
	 * and feed document.write() data back to the parser. */

	return parser;
}

void
done_sgml_parser(struct sgml_parser *parser)
{
	done_dom_stack(&parser->stack);
	done_uri(parser->uri);
	mem_free(parser);
}

/* FIXME: Make it possible to push variable number of strings (even nested
 * while parsing another string) so that we can feed back output of stuff
 * like ECMAScripts document.write(). */
struct dom_node *
parse_sgml(struct sgml_parser *parser, struct string *buffer)
{
	unsigned char *source = buffer->source;
	unsigned char *end = source + buffer->length;

	if (!parser->root) {
		parser->root = add_sgml_document(&parser->stack, parser->uri);
		if (!parser->root)
			return NULL;

		get_dom_stack_top(&parser->stack)->immutable = 1;
	}

	init_scanner(&parser->scanner, &sgml_scanner_info, source, end);

	/* FIXME: Make parse_sgml_document() return an error code. */
	parse_sgml_document(&parser->stack, &parser->scanner);

	/* FIXME: Return the 'bottom' node that was added by the parser. */
	return parser->root;
}
