/* DOM node selection */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/css/scanner.h"
#include "document/dom/dom.h"
#include "document/dom/node.h"
#include "document/dom/select.h"
#include "document/dom/stack.h"
#include "util/memory.h"
#include "util/scanner.h"
#include "util/string.h"


static enum dom_select_pseudo
get_dom_select_pseudo(struct scanner_token *token)
{
	static struct {
		struct dom_string string;
		enum dom_select_pseudo pseudo;
	} pseudo_info[] = {

#define INIT_DOM_SELECT_PSEUDO_STRING(str, type) \
	{ INIT_DOM_STRING(str, -1), DOM_SELECT_PSEUDO_##type }

	INIT_DOM_SELECT_PSEUDO_STRING("first-line",	FIRST_LINE),
	INIT_DOM_SELECT_PSEUDO_STRING("first-letter",	FIRST_LETTER),
	INIT_DOM_SELECT_PSEUDO_STRING("selection",	SELECTION),
	INIT_DOM_SELECT_PSEUDO_STRING("after",		AFTER),
	INIT_DOM_SELECT_PSEUDO_STRING("before",		BEFORE),
	INIT_DOM_SELECT_PSEUDO_STRING("link",		LINK),
	INIT_DOM_SELECT_PSEUDO_STRING("visited",	VISITED),
	INIT_DOM_SELECT_PSEUDO_STRING("active",		ACTIVE),
	INIT_DOM_SELECT_PSEUDO_STRING("hover",		HOVER),
	INIT_DOM_SELECT_PSEUDO_STRING("focus",		FOCUS),
	INIT_DOM_SELECT_PSEUDO_STRING("target",		TARGET),
	INIT_DOM_SELECT_PSEUDO_STRING("enabled",	ENABLED),
	INIT_DOM_SELECT_PSEUDO_STRING("disabled",	DISABLED),
	INIT_DOM_SELECT_PSEUDO_STRING("checked",	CHECKED),
	INIT_DOM_SELECT_PSEUDO_STRING("indeterminate",	INDETERMINATE),

	/* Content pseudo-classes: */

	INIT_DOM_SELECT_PSEUDO_STRING("contains",	CONTAINS),

	/* Structural pseudo-classes: */

	INIT_DOM_SELECT_PSEUDO_STRING("nth-child",	NTH_CHILD),
	INIT_DOM_SELECT_PSEUDO_STRING("nth-last-child",	NTH_LAST_CHILD),
	INIT_DOM_SELECT_PSEUDO_STRING("first-child",	FIRST_CHILD),
	INIT_DOM_SELECT_PSEUDO_STRING("last-child",	LAST_CHILD),
	INIT_DOM_SELECT_PSEUDO_STRING("only-child",	ONLY_CHILD),

	INIT_DOM_SELECT_PSEUDO_STRING("nth-of-type",	NTH_TYPE),
	INIT_DOM_SELECT_PSEUDO_STRING("nth-last-of-type",NTH_LAST_TYPE),
	INIT_DOM_SELECT_PSEUDO_STRING("first-of-type",	FIRST_TYPE),
	INIT_DOM_SELECT_PSEUDO_STRING("last-of-type",	LAST_TYPE),
	INIT_DOM_SELECT_PSEUDO_STRING("only-of-type",	ONLY_TYPE),

	INIT_DOM_SELECT_PSEUDO_STRING("root",		ROOT),
	INIT_DOM_SELECT_PSEUDO_STRING("empty",		EMPTY),

#undef INIT_DOM_SELECT_PSEUDO_STRING

	};
	struct dom_string string;
	int i;

	set_dom_string(&string, token->string, token->length);

	for (i = 0; i < sizeof_array(pseudo_info); i++)
		if (!dom_string_casecmp(&pseudo_info[i].string, &string))
			return pseudo_info[i].pseudo;

	return DOM_SELECT_PSEUDO_UNKNOWN;
}


static enum dom_exception_code
parse_dom_select_attribute(struct dom_select_node *sel, struct scanner *scanner)
{
	struct scanner_token *token = get_scanner_token(scanner);

	if (token->type != '[')
		return DOM_ERR_INVALID_STATE;

	token = get_next_scanner_token(scanner);
	if (!token || token->type != CSS_TOKEN_IDENT)
		return DOM_ERR_SYNTAX;

	set_dom_string(&sel->node.string, token->string, token->length);

	token = get_next_scanner_token(scanner);
	if (!token) return DOM_ERR_SYNTAX;

	switch (token->type) {
	case ']':
		sel->match.attribute |= DOM_SELECT_ATTRIBUTE_ANY;
		return DOM_ERR_NONE;

	case CSS_TOKEN_SELECT_SPACE_LIST:
		sel->match.attribute |= DOM_SELECT_ATTRIBUTE_SPACE_LIST;
		break;

	case CSS_TOKEN_SELECT_HYPHEN_LIST:
		sel->match.attribute |= DOM_SELECT_ATTRIBUTE_HYPHEN_LIST;
		break;

	case CSS_TOKEN_SELECT_BEGIN:
		sel->match.attribute |= DOM_SELECT_ATTRIBUTE_BEGIN;
		break;

	case CSS_TOKEN_SELECT_END:
		sel->match.attribute |= DOM_SELECT_ATTRIBUTE_END;
		break;

	case CSS_TOKEN_SELECT_CONTAINS:
		sel->match.attribute |= DOM_SELECT_ATTRIBUTE_CONTAINS;
		break;

	default:
		return DOM_ERR_SYNTAX;
	}

	token = get_next_scanner_token(scanner);
	if (!token) return DOM_ERR_SYNTAX;

	switch (token->type) {
	case CSS_TOKEN_IDENT:
	case CSS_TOKEN_STRING:
		set_dom_string(&sel->node.data.attribute.value, token->string, token->length);
		break;

	default:
		return DOM_ERR_SYNTAX;
	}

	token = get_next_scanner_token(scanner);
	if (token && token->type == ']')
		return DOM_ERR_NONE;

	return DOM_ERR_SYNTAX;
}

/* Parse:
 *
 *   0n+1 / 1		
 *   2n+0 / 2n	
 *   2n+1 	
 *  -0n+2		
 *  -0n+1 / -1		
 *   1n+0 / n+0 / n	
 *   0n+0		
 */

static size_t
get_scanner_token_number(struct scanner_token *token)
{
	size_t number = 0;

	while (token->length > 0 && isdigit(token->string[0])) {
		size_t old_number = number;

		number *= 10;

		/* -E2BIG */
		if (old_number > number)
			return -1;

		number += token->string[0] - '0';
		token->string++, token->length--;
	}

	return number;
}

static enum dom_exception_code
parse_dom_select_nth_arg(struct dom_select_nth_match *nth, struct scanner *scanner)
{
	struct scanner_token *token = get_next_scanner_token(scanner);
	int sign = 1;
	int number = -1;

	if (!token || token->type != '(')
		return DOM_ERR_SYNTAX;

	token = get_next_scanner_token(scanner);
	if (!token)
		return DOM_ERR_SYNTAX;

	switch (token->type) {
	case CSS_TOKEN_IDENT:
		if (scanner_token_contains(token, "even")) {
			nth->step = 2;
			nth->index = 0;

		} else if (scanner_token_contains(token, "odd")) {
			nth->step = 2;
			nth->index = 1;

		} else {
			/* Check for 'n' ident below. */
			break;
		}

		if (skip_css_tokens(scanner, ')'))
			return DOM_ERR_NONE;

		return DOM_ERR_SYNTAX;

	case '-':
		sign = -1;

		token = get_next_scanner_token(scanner);
		if (!token) return DOM_ERR_SYNTAX;

		if (token->type != CSS_TOKEN_IDENT)
			break;

		if (token->type != CSS_TOKEN_NUMBER)
			return DOM_ERR_SYNTAX;
		/* Fall-through */

	case CSS_TOKEN_NUMBER:
		number = get_scanner_token_number(token);
		if (number < 0)
			return DOM_ERR_INVALID_STATE;

		token = get_next_scanner_token(scanner);
		if (!token) return DOM_ERR_SYNTAX;
		break;

	default:
		return DOM_ERR_SYNTAX;
	}

	/* The rest can contain n+ part */
	switch (token->type) {
	case CSS_TOKEN_IDENT:
		if (!scanner_token_contains(token, "n"))
			return DOM_ERR_SYNTAX;

		nth->step = sign * number;

		token = get_next_scanner_token(scanner);
		if (!token) return DOM_ERR_SYNTAX;

		if (token->type != '+')
			break;

		token = get_next_scanner_token(scanner);
		if (!token) return DOM_ERR_SYNTAX;

		if (token->type != CSS_TOKEN_NUMBER)
			break;

		number = get_scanner_token_number(token);
		if (number < 0)
			return DOM_ERR_INVALID_STATE;

		nth->index = sign * number;
		break;

	default:
		nth->step  = 0;
		nth->index = sign * number;
	}

	if (skip_css_tokens(scanner, ')'))
		return DOM_ERR_NONE;

	return DOM_ERR_SYNTAX;
}

static enum dom_exception_code
parse_dom_select_pseudo(struct dom_select *select, struct dom_select_node *sel,
			struct scanner *scanner)
{
	struct scanner_token *token = get_scanner_token(scanner);
	enum dom_select_pseudo pseudo;
	enum dom_exception_code code;

	/* Skip double :'s in front of some pseudo's */
	do {
		token = get_next_scanner_token(scanner);
	} while (token && token->type == ':');

	if (!token || token->type != CSS_TOKEN_IDENT)
		return DOM_ERR_SYNTAX;

	pseudo = get_dom_select_pseudo(token);
	switch (pseudo) {
	case DOM_SELECT_PSEUDO_UNKNOWN:
		return DOM_ERR_NOT_FOUND;

	case DOM_SELECT_PSEUDO_CONTAINS:
		/* FIXME: E:contains("text") */
		break;

	case DOM_SELECT_PSEUDO_NTH_CHILD:
	case DOM_SELECT_PSEUDO_NTH_LAST_CHILD:
		code = parse_dom_select_nth_arg(&sel->nth_child, scanner);
		if (code != DOM_ERR_NONE)
			return code;

		sel->match.element |= DOM_SELECT_ELEMENT_NTH_CHILD;
		break;

	case DOM_SELECT_PSEUDO_FIRST_CHILD:
		sel->match.element |= DOM_SELECT_ELEMENT_NTH_CHILD;
		set_dom_select_nth_match(&sel->nth_child, 0, 1);
		break;

	case DOM_SELECT_PSEUDO_LAST_CHILD:
		sel->match.element |= DOM_SELECT_ELEMENT_NTH_CHILD;
		set_dom_select_nth_match(&sel->nth_child, 0, -1);
		break;

	case DOM_SELECT_PSEUDO_ONLY_CHILD:
		sel->match.element |= DOM_SELECT_ELEMENT_NTH_CHILD;
		set_dom_select_nth_match(&sel->nth_child, 0, 0);
		break;

	case DOM_SELECT_PSEUDO_NTH_TYPE:
	case DOM_SELECT_PSEUDO_NTH_LAST_TYPE:
		code = parse_dom_select_nth_arg(&sel->nth_type, scanner);
		if (code != DOM_ERR_NONE)
			return code;

		sel->match.element |= DOM_SELECT_ELEMENT_NTH_TYPE;
		break;

	case DOM_SELECT_PSEUDO_FIRST_TYPE:
		sel->match.element |= DOM_SELECT_ELEMENT_NTH_TYPE;
		set_dom_select_nth_match(&sel->nth_type, 0, 1);
		break;

	case DOM_SELECT_PSEUDO_LAST_TYPE:
		sel->match.element |= DOM_SELECT_ELEMENT_NTH_TYPE;
		set_dom_select_nth_match(&sel->nth_type, 0, -1);
		break;

	case DOM_SELECT_PSEUDO_ONLY_TYPE:
		sel->match.element |= DOM_SELECT_ELEMENT_NTH_TYPE;
		set_dom_select_nth_match(&sel->nth_type, 0, 0);
		break;

	case DOM_SELECT_PSEUDO_ROOT:
		sel->match.element |= DOM_SELECT_ELEMENT_ROOT;
		break;

	case DOM_SELECT_PSEUDO_EMPTY:
		sel->match.element |= DOM_SELECT_ELEMENT_EMPTY;
		break;

	default:
		/* It's a bitflag! */
		select->pseudo |= pseudo;
	}

	return DOM_ERR_NONE;
}

#define get_element_relation(sel) \
	((sel)->match.element & DOM_SELECT_RELATION_FLAGS)

static enum dom_exception_code
parse_dom_select(struct dom_select *select, unsigned char *string, int length)
{
	struct dom_stack stack;
	struct scanner scanner;
	struct dom_select_node sel;

	init_scanner(&scanner, &css_scanner_info, string, string + length);
	init_dom_stack(&stack, select, 0, 1);

	memset(&sel, 0, sizeof(sel));

	while (scanner_has_tokens(&scanner)) {
		struct scanner_token *token = get_scanner_token(&scanner);
		enum dom_exception_code code;
		struct dom_select_node *select_node;

		assert(token);

		if (token->type == '{'
		    || token->type == '}'
		    || token->type == ';'
		    || token->type == ',')
			break;

		/* Examine the selector fragment */

		switch (token->type) {
		case CSS_TOKEN_IDENT:
			sel.node.type = DOM_NODE_ELEMENT;
			set_dom_string(&sel.node.string, token->string, token->length);
			if (token->length == 1 && token->string[0] == '*')
				sel.match.element |= DOM_SELECT_ELEMENT_UNIVERSAL;
			break;

		case CSS_TOKEN_HASH:
		case CSS_TOKEN_HEX_COLOR:
			/* ID fragment */
			sel.node.type = DOM_NODE_ATTRIBUTE;
			sel.match.attribute |= DOM_SELECT_ATTRIBUTE_ID;
			/* Skip the leading '#'. */
			token->string++, token->length--;
			break;

		case '[':
			sel.node.type = DOM_NODE_ATTRIBUTE;
			code = parse_dom_select_attribute(&sel, &scanner);
			if (code != DOM_ERR_NONE)
				return code;
			break;

		case '.':
			token = get_next_scanner_token(&scanner);
			if (!token || token->type != CSS_TOKEN_IDENT)
				return DOM_ERR_SYNTAX;

			sel.node.type = DOM_NODE_ATTRIBUTE;
			sel.match.attribute |= DOM_SELECT_ATTRIBUTE_SPACE_LIST;
			set_dom_string(&sel.node.string, "class", -1); 
			set_dom_string(&sel.node.data.attribute.value, token->string, token->length); 
			break;

		case ':':
			code = parse_dom_select_pseudo(select, &sel, &scanner);
			if (code != DOM_ERR_NONE)
				return code;
			break;

		case '>':
			if (get_element_relation(&sel))
				return DOM_ERR_SYNTAX;
			sel.match.element |= DOM_SELECT_RELATION_DIRECT_CHILD;
			break;

		case '+':
			if (get_element_relation(&sel))
				return DOM_ERR_SYNTAX;
			sel.match.element |= DOM_SELECT_RELATION_DIRECT_ADJACENT;
			break;

		case '~':
			if (get_element_relation(&sel))
				return DOM_ERR_SYNTAX;
			sel.match.element |= DOM_SELECT_RELATION_INDIRECT_ADJACENT;
			break;

		default:
			return DOM_ERR_SYNTAX;
		}

		skip_scanner_token(&scanner);

		if (sel.node.type == DOM_NODE_UNKNOWN)
			continue;

		WDBG("Adding %s: %.*s", (sel.node.type == DOM_NODE_ELEMENT) ? "element" : "attr", sel.node.string.length, sel.node.string.string);
		/* FIXME */
		select_node = mem_calloc(1, sizeof(*select_node));
		copy_struct(select_node, &sel);

		if (select_node->node.parent) {
			struct dom_node *node = &select_node->node;
			struct dom_node *parent = node->parent;
			struct dom_node_list **list = get_dom_node_list(parent, node);
			int sort = (node->type == DOM_NODE_ATTRIBUTE);
			int index;

			assertm(list, "Adding node to bad parent",
				get_dom_node_type_name(node->type),
				get_dom_node_type_name(parent->type));

			index = *list && (*list)->size > 0 && sort
				? get_dom_node_map_index(*list, node) : -1;

			if (!add_to_dom_node_list(list, node, index)) {
				done_dom_node(node);
				return DOM_ERR_INVALID_STATE;
			}
		} else {
			assert(!select->selector);
			select->selector = select_node;
		}

		memset(&sel, 0, sizeof(sel));
		sel.node.parent = &select_node->node;
	}

	if (select->selector)
		return DOM_ERR_NONE;

	return DOM_ERR_INVALID_STATE;
}

struct dom_select *
init_dom_select(enum dom_select_syntax syntax,
		unsigned char *string, int length)
{
	struct dom_select *select = mem_calloc(1, sizeof(select));
	enum dom_exception_code code;

	code = parse_dom_select(select, string, length);
	if (code == DOM_ERR_NONE)
		return select;

	done_dom_select(select);

	return NULL;
}

void
done_dom_select(struct dom_select *select)
{
	if (select->selector) {
		struct dom_node *node = (struct dom_node *) select->selector;

		done_dom_node(node);
	}

	mem_free(select);
}


struct dom_select_data {
	struct dom_stack stack;
	struct dom_select *select;
	struct dom_node_list *list;
};

struct dom_select_state {
	struct dom_node *node;
};


static int
compare_element_type(struct dom_node *node1, struct dom_node *node2)
{
	/* Assuming the same document type */
	if (node1->data.element.type
	    && node2->data.element.type
	    && node1->data.element.type == node2->data.element.type)
		return 0;

	return dom_string_casecmp(&node1->string, &node2->string);
}

static struct dom_select_node *
get_child_dom_select_node(struct dom_select_node *selector,
			  enum dom_node_type type)
{
	struct dom_node_list *children = selector->node.data.element.children;
	struct dom_node *node;
	int index;

	if (!children)
		return NULL;

	foreach_dom_node (children, node, index) {
		if (node->type == type)
			return (struct dom_select_node *) node;
	}

	return NULL;
}


#define has_attribute_match(selector, name) \
	((selector)->match.attribute & (name))

static int
match_attribute_selectors(struct dom_select_node *base, struct dom_node *node)
{
	struct dom_node_list *attrs = node->data.element.map;
	struct dom_node_list *selnodes = base->node.data.element.map;
	struct dom_node *selnode;
	size_t index;

	assert(base->node.type == DOM_NODE_ELEMENT
	       && node->type == DOM_NODE_ELEMENT);

	if (!selnodes)
		return 1;

	if (!attrs)
		return 0;

	foreach_dom_node (selnodes, selnode, index) {
		struct dom_select_node *selector = (void *) selnode;
		struct dom_node *attr;
		struct dom_string *value;
		struct dom_string *selvalue;

		if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_ID)) {
			size_t idindex;

			attr = NULL;
			foreach_dom_node (attrs, attr, idindex) {
				if (attr->data.attribute.id)
					break;
			}

			if (!is_dom_node_list_member(attrs, idindex))
				attr = NULL;

		} else {
			attr = get_dom_node_map_entry(attrs, DOM_NODE_ATTRIBUTE,
						      selnode->data.attribute.type,
						      &selnode->string);
		}

		if (!attr)
			return 0;

		if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_ANY))
			continue;

		value	 = &attr->data.attribute.value;
		selvalue = &selnode->data.attribute.value;

		if (value->length < selvalue->length)
			return 0;

		if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_EXACT)
		    && dom_string_casecmp(value, selvalue))
			return 0;

		if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_BEGIN))
			return 0;

		if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_END))
			return 0;

		if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_SPACE_LIST))
			return 0;

		if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_HYPHEN_LIST))
			return 0;

		if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_CONTAINS))
			return 0;
	}

	return 1;
}

#define has_element_match(selector, name) \
	((selector)->match.element & (name))

static void
dom_select_push_element(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_select_data *select_data = stack->data;
	struct dom_stack_state *state;
	int pos;

	WDBG("Push element %.*s.", node->string.length, node->string.string);

	foreach_dom_stack_state(&select_data->stack, state, pos) {
		struct dom_select_node *selector = (void *) state->node;

		/* Match the node. */
		if (!has_element_match(selector, DOM_SELECT_ELEMENT_UNIVERSAL)
		    && compare_element_type(&selector->node, node))
			continue;

		switch (get_element_relation(selector)) {
		case DOM_SELECT_RELATION_DIRECT_CHILD:		/* E > F */
			/* node->parent */
			/* Check all states to see if node->parent is there
			 * and for the right reasons. */
			break;

		case DOM_SELECT_RELATION_DIRECT_ADJACENT:	/* E + F */
			/* Get preceding node to see if it is on the stack. */
			break;

		case DOM_SELECT_RELATION_INDIRECT_ADJACENT:	/* E ~ F */
			/* Check all states with same depth? */
			break;

		case DOM_SELECT_RELATION_DESCENDANT:		/* E   F */
		default:
			break;
		}

		/* Roots don't have parent nodes. */
		if (has_element_match(selector, DOM_SELECT_ELEMENT_ROOT)
		    && node->parent)
			continue;

		if (has_element_match(selector, DOM_SELECT_ELEMENT_EMPTY)
		    && node->data.element.map->size > 0)
			continue;

		if (has_element_match(selector, DOM_SELECT_ELEMENT_NTH_CHILD)) {
			/* FIXME */
			continue;
		}

		if (has_element_match(selector, DOM_SELECT_ELEMENT_NTH_TYPE)) {
			/* FIXME */
			continue;
		}

		/* Check attribute selectors. */
		if (selector->node.data.element.map
		    && !match_attribute_selectors(selector, node))
			continue;

		WDBG("Matched element: %.*s.", node->string.length, node->string.string);
		/* This node is matched, so push the next selector node to
		 * match on the stack. */
		selector = get_child_dom_select_node(selector, DOM_NODE_ELEMENT);
		if (selector)
			push_dom_node(&select_data->stack, &selector->node);
	}
}

static void
dom_select_pop_element(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_select_data *select_data = stack->data;
	struct dom_stack_state *state;
	int index;

	WDBG("Pop element: %.*s", node->string.length, node->string.string);
	stack = &select_data->stack;

	foreachback_dom_stack_state (stack, state, index) {
		struct dom_select_node *selector = (void *) state->node;
		struct dom_select_state *select_state;

		select_state = get_dom_stack_state_data(stack, state);
		if (select_state->node == node) {
			pop_dom_state(stack, state);
			WDBG("Remove element.");
			continue;
		}

		/* Pop states that no longer lives up to a relation. */
		switch (get_element_relation(selector)) {
		case DOM_SELECT_RELATION_DIRECT_CHILD:		/* E > F */
		case DOM_SELECT_RELATION_DIRECT_ADJACENT:	/* E + F */
		case DOM_SELECT_RELATION_INDIRECT_ADJACENT:	/* E ~ F */
		case DOM_SELECT_RELATION_DESCENDANT:		/* E   F */
		default:
			break;
		}
	}
}

static void
dom_select_push_text(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_select_data *select_data = stack->data;
	struct dom_stack_state *state = get_dom_stack_top(&select_data->stack);
	struct dom_select_node *selector = (void *) state->node;
	struct dom_select_node *text_sel = get_child_dom_select_node(selector, DOM_NODE_TEXT);
	struct dom_string *text;

	WDBG("Text node: %d chars", node->string.length);

	if (!text_sel)
		return;

	text = &text_sel->node.string;

	switch (node->type) {
	case DOM_NODE_TEXT:
	case DOM_NODE_CDATA_SECTION:
	case DOM_NODE_ENTITY_REFERENCE:
		break;
	default:
		ERROR("Unhandled type");
	}
}

static struct dom_stack_context_info dom_select_context_info = {
	/* Push: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ dom_select_push_element,
		/* DOM_NODE_ATTRIBUTE		*/ NULL,
		/* DOM_NODE_TEXT		*/ dom_select_push_text,
		/* DOM_NODE_CDATA_SECTION	*/ dom_select_push_text,
		/* DOM_NODE_ENTITY_REFERENCE	*/ dom_select_push_text,
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
		/* DOM_NODE_ELEMENT		*/ dom_select_pop_element,
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

struct dom_node_list *
select_dom_nodes(struct dom_select *select, struct dom_node *root)
{
	struct dom_select_data select_data;
	struct dom_stack stack;
	size_t obj_size = sizeof(struct dom_select_state);

	memset(&select_data, 0, sizeof(select_data));

	select_data.select = select;;

	init_dom_stack(&stack, &select_data, 0, 1);
	add_dom_stack_context(&stack, &select_data,
			      &dom_select_context_info);

	init_dom_stack(&select_data.stack, &select_data, obj_size, 1);

	if (push_dom_node(&select_data.stack, &select->selector->node)) {
		get_dom_stack_top(&select_data.stack)->immutable = 1;
		walk_dom_nodes(&stack, root);
	}

	done_dom_stack(&select_data.stack);
	done_dom_stack(&stack);

	return select_data.list;
}
