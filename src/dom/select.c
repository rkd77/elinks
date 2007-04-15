/* DOM node selection */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "dom/css/scanner.h"
#include "dom/code.h"
#include "dom/node.h"
#include "dom/scanner.h"
#include "dom/select.h"
#include "dom/stack.h"
#include "dom/string.h"
#include "util/memory.h"


/* Selector parsing: */

/* Maps the content of a scanner token to a pseudo-class or -element ID. */
static enum dom_select_pseudo
get_dom_select_pseudo(struct dom_scanner_token *token)
{
	static struct {
		struct dom_string string;
		enum dom_select_pseudo pseudo;
	} pseudo_info[] = {

#define INIT_DOM_SELECT_PSEUDO_STRING(str, type) \
	{ STATIC_DOM_STRING(str), DOM_SELECT_PSEUDO_##type }

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
	int i;

	for (i = 0; i < sizeof_array(pseudo_info); i++)
		if (!dom_string_casecmp(&pseudo_info[i].string, &token->string))
			return pseudo_info[i].pseudo;

	return DOM_SELECT_PSEUDO_UNKNOWN;
}

/* Parses attribute selector. For example '[foo="bar"]' or '[foo|="boo"]'. */
static enum dom_code
parse_dom_select_attribute(struct dom_select_node *sel, struct dom_scanner *scanner)
{
	struct dom_scanner_token *token = get_dom_scanner_token(scanner);

	/* Get '['. */

	if (token->type != '[')
		return DOM_CODE_SYNTAX_ERR;

	/* Get the attribute name. */

	token = get_next_dom_scanner_token(scanner);
	if (!token || token->type != CSS_TOKEN_IDENT)
		return DOM_CODE_SYNTAX_ERR;

	copy_dom_string(&sel->node.string, &token->string);

	/* Get the optional '=' combo or ending ']'. */

	token = get_next_dom_scanner_token(scanner);
	if (!token) return DOM_CODE_SYNTAX_ERR;

	switch (token->type) {
	case ']':
		sel->match.attribute |= DOM_SELECT_ATTRIBUTE_ANY;
		return DOM_CODE_OK;

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
		return DOM_CODE_SYNTAX_ERR;
	}

	/* Get the required value. */

	token = get_next_dom_scanner_token(scanner);
	if (!token) return DOM_CODE_SYNTAX_ERR;

	switch (token->type) {
	case CSS_TOKEN_IDENT:
	case CSS_TOKEN_STRING:
		copy_dom_string(&sel->node.data.attribute.value, &token->string);
		break;

	default:
		return DOM_CODE_SYNTAX_ERR;
	}

	/* Get the ending ']'. */

	token = get_next_dom_scanner_token(scanner);
	if (token && token->type == ']')
		return DOM_CODE_OK;

	return DOM_CODE_SYNTAX_ERR;
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

/* FIXME: Move somewhere else? dom/scanner.h? */
static size_t
get_scanner_token_number(struct dom_scanner_token *token)
{
	size_t number = 0;

	while (token->string.length > 0 && isdigit(token->string.string[0])) {
		size_t old_number = number;

		number *= 10;

		/* -E2BIG */
		if (old_number > number)
			return -1;

		number += token->string.string[0] - '0';
		skip_dom_scanner_token_char(token);
	}

	return number;
}

/* Parses the '(...)' part of ':nth-of-type(...)' and ':nth-child(...)'. */
static enum dom_code
parse_dom_select_nth_arg(struct dom_select_nth_match *nth, struct dom_scanner *scanner)
{
	struct dom_scanner_token *token = get_next_dom_scanner_token(scanner);
	int sign = 1;
	int number = -1;

	if (!token || token->type != '(')
		return DOM_CODE_SYNTAX_ERR;

	token = get_next_dom_scanner_token(scanner);
	if (!token)
		return DOM_CODE_SYNTAX_ERR;

	switch (token->type) {
	case CSS_TOKEN_IDENT:
		if (dom_scanner_token_contains(token, "even")) {
			nth->step = 2;
			nth->index = 0;

		} else if (dom_scanner_token_contains(token, "odd")) {
			nth->step = 2;
			nth->index = 1;

		} else {
			/* Check for 'n' ident below. */
			break;
		}

		if (skip_css_tokens(scanner, ')'))
			return DOM_CODE_OK;

		return DOM_CODE_SYNTAX_ERR;

	case '-':
		sign = -1;

		token = get_next_dom_scanner_token(scanner);
		if (!token) return DOM_CODE_SYNTAX_ERR;

		if (token->type != CSS_TOKEN_IDENT)
			break;

		if (token->type != CSS_TOKEN_NUMBER)
			return DOM_CODE_SYNTAX_ERR;
		/* Fall-through */

	case CSS_TOKEN_NUMBER:
		number = get_scanner_token_number(token);
		if (number < 0)
			return DOM_CODE_VALUE_ERR;

		token = get_next_dom_scanner_token(scanner);
		if (!token) return DOM_CODE_SYNTAX_ERR;
		break;

	default:
		return DOM_CODE_SYNTAX_ERR;
	}

	/* The rest can contain n+ part */
	switch (token->type) {
	case CSS_TOKEN_IDENT:
		if (!dom_scanner_token_contains(token, "n"))
			return DOM_CODE_SYNTAX_ERR;

		nth->step = sign * number;

		token = get_next_dom_scanner_token(scanner);
		if (!token) return DOM_CODE_SYNTAX_ERR;

		if (token->type != '+')
			break;

		token = get_next_dom_scanner_token(scanner);
		if (!token) return DOM_CODE_SYNTAX_ERR;

		if (token->type != CSS_TOKEN_NUMBER)
			break;

		number = get_scanner_token_number(token);
		if (number < 0)
			return DOM_CODE_VALUE_ERR;

		nth->index = sign * number;
		break;

	default:
		nth->step  = 0;
		nth->index = sign * number;
	}

	if (skip_css_tokens(scanner, ')'))
		return DOM_CODE_OK;

	return DOM_CODE_SYNTAX_ERR;
}

/* Parse a pseudo-class or -element with the syntax: ':<ident>'. */
static enum dom_code
parse_dom_select_pseudo(struct dom_select *select, struct dom_select_node *sel,
			struct dom_scanner *scanner)
{
	struct dom_scanner_token *token = get_dom_scanner_token(scanner);
	enum dom_select_pseudo pseudo;
	enum dom_code code;

	/* Skip double :'s in front of some pseudo's (::first-line, etc.) */
	do {
		token = get_next_dom_scanner_token(scanner);
	} while (token && token->type == ':');

	if (!token || token->type != CSS_TOKEN_IDENT)
		return DOM_CODE_SYNTAX_ERR;

	pseudo = get_dom_select_pseudo(token);
	switch (pseudo) {
	case DOM_SELECT_PSEUDO_UNKNOWN:
		return DOM_CODE_ERR;

	case DOM_SELECT_PSEUDO_CONTAINS:
		/* FIXME: E:contains("text") */
		break;

	case DOM_SELECT_PSEUDO_NTH_CHILD:
	case DOM_SELECT_PSEUDO_NTH_LAST_CHILD:
		code = parse_dom_select_nth_arg(&sel->nth_child, scanner);
		if (code != DOM_CODE_OK)
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
		if (code != DOM_CODE_OK)
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

	return DOM_CODE_OK;
}

/* The element relation flags are mutual exclusive. This macro can be used
 * for checking if anyflag is set. */
#define get_element_relation(sel) \
	((sel)->match.element & DOM_SELECT_RELATION_FLAGS)

/* Parse a CSS3 selector and add selector nodes to the @select struct. */
static enum dom_code
parse_dom_select(struct dom_select *select, struct dom_stack *stack,
		 struct dom_string *string)
{
	struct dom_scanner scanner;
	struct dom_select_node sel;

	init_dom_scanner(&scanner, &dom_css_scanner_info, string, 0, 0, 1, 0, 0);

	memset(&sel, 0, sizeof(sel));

	while (dom_scanner_has_tokens(&scanner)) {
		struct dom_scanner_token *token = get_dom_scanner_token(&scanner);
		enum dom_code code;
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
			copy_dom_string(&sel.node.string, &token->string);
			if (dom_scanner_token_contains(token, "*"))
				sel.match.element |= DOM_SELECT_ELEMENT_UNIVERSAL;
			break;

		case CSS_TOKEN_HASH:
		case CSS_TOKEN_HEX_COLOR:
			/* ID fragment */
			sel.node.type = DOM_NODE_ATTRIBUTE;
			sel.match.attribute |= DOM_SELECT_ATTRIBUTE_ID;
			/* Skip the leading '#'. */
			skip_dom_scanner_token_char(token);
			break;

		case '[':
			sel.node.type = DOM_NODE_ATTRIBUTE;
			code = parse_dom_select_attribute(&sel, &scanner);
			if (code != DOM_CODE_OK)
				return code;
			break;

		case '.':
			token = get_next_dom_scanner_token(&scanner);
			if (!token || token->type != CSS_TOKEN_IDENT)
				return DOM_CODE_SYNTAX_ERR;

			sel.node.type = DOM_NODE_ATTRIBUTE;
			sel.match.attribute |= DOM_SELECT_ATTRIBUTE_SPACE_LIST;
			set_dom_string(&sel.node.string, "class", -1);
			copy_dom_string(&sel.node.data.attribute.value, &token->string);
			break;

		case ':':
			code = parse_dom_select_pseudo(select, &sel, &scanner);
			if (code != DOM_CODE_OK)
				return code;
			break;

		case '>':
			if (get_element_relation(&sel) != DOM_SELECT_RELATION_DESCENDANT)
				return DOM_CODE_SYNTAX_ERR;
			sel.match.element |= DOM_SELECT_RELATION_DIRECT_CHILD;
			break;

		case '+':
			if (get_element_relation(&sel) != DOM_SELECT_RELATION_DESCENDANT)
				return DOM_CODE_SYNTAX_ERR;
			sel.match.element |= DOM_SELECT_RELATION_DIRECT_ADJACENT;
			break;

		case '~':
			if (get_element_relation(&sel) != DOM_SELECT_RELATION_DESCENDANT)
				return DOM_CODE_SYNTAX_ERR;
			sel.match.element |= DOM_SELECT_RELATION_INDIRECT_ADJACENT;
			break;

		default:
			return DOM_CODE_SYNTAX_ERR;
		}

		skip_dom_scanner_token(&scanner);

		if (sel.node.type == DOM_NODE_UNKNOWN)
			continue;

		select_node = mem_calloc(1, sizeof(*select_node));
		copy_struct(select_node, &sel);

		if (!dom_stack_is_empty(stack)) {
			struct dom_node *node = &select_node->node;
			struct dom_node *parent = get_dom_stack_top(stack)->node;
			struct dom_node_list **list = get_dom_node_list(parent, node);
			int sort = (node->type == DOM_NODE_ATTRIBUTE);
			int index;

			assertm(list != NULL, "Adding node to bad parent [%d -> %d]",
				node->type, parent->type);

			index = *list && (*list)->size > 0 && sort
				? get_dom_node_map_index(*list, node) : -1;

			if (!add_to_dom_node_list(list, node, index)) {
				done_dom_node(node);
				return DOM_CODE_ALLOC_ERR;
			}

			node->parent = parent;

		} else {
			assert(!select->selector);
			select->selector = select_node;
		}

		code = push_dom_node(stack, &select_node->node);
		if (code != DOM_CODE_OK)
			return code;

		if (select_node->node.type != DOM_NODE_ELEMENT)
			pop_dom_node(stack);

		memset(&sel, 0, sizeof(sel));
	}

	if (select->selector)
		return DOM_CODE_OK;

	return DOM_CODE_ERR;
}

/* Basically this is just a wrapper for parse_dom_select() to ease error
 * handling. */
struct dom_select *
init_dom_select(enum dom_select_syntax syntax, struct dom_string *string)
{
	struct dom_select *select = mem_calloc(1, sizeof(select));
	struct dom_stack stack;
	enum dom_code code;

	init_dom_stack(&stack, DOM_STACK_FLAG_NONE);
	add_dom_stack_tracer(&stack, "init-select: ");

	code = parse_dom_select(select, &stack, string);
	done_dom_stack(&stack);

	if (code == DOM_CODE_OK)
		return select;

	done_dom_select(select);

	return NULL;
}

void
done_dom_select(struct dom_select *select)
{
	if (select->selector) {
		struct dom_node *node = (struct dom_node *) select->selector;

		/* This will recursively free all children select nodes. */
		done_dom_node(node);
	}

	mem_free(select);
}


/* DOM node selection: */

/* This struct stores data related to the 'application' of a DOM selector
 * on a DOM tree or stream. */
struct dom_select_data {
	/* The selector matching stack. The various selector nodes are pushed
	 * onto this stack as they are matched (and later popped when they are
	 * no longer 'reachable', that is, has been popped from the DOM tree or
	 * stream. This way the selector can match each selector node multiple
	 * times and the selection is a simple matter of matching the current
	 * node against each state on this stack. */
	struct dom_stack stack;

	/* Reference to the selector. */
	struct dom_select *select;

	/* The list of nodes who have been matched / selected. */
	struct dom_node_list *list;
};

/* This state struct is used for the select data stack and holds info about the
 * node that was matched. */
struct dom_select_state {
	/* The matched node. This is always an element node. */
	struct dom_node *node;
};

/* Get a child node of a given type. By design, a selector node can
 * only have one child per type of node. */
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
match_attribute_value(struct dom_select_node *selector, struct dom_node *node)
{
	struct dom_string str;
	struct dom_string *selvalue = &selector->node.data.attribute.value;
	struct dom_string *value = &node->data.attribute.value;
	unsigned char separator;
	int do_compare;

	assert(selvalue->length);

	/* The attribute selector value should atleast be contained in the
	 * attribute value. */
	if (value->length < selvalue->length)
		return 0;

	/* The following three matching methods requires the selector value to
	 * match a substring at a well-defined offset. */

	if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_EXACT)) {
		return !dom_string_casecmp(value, selvalue);
	}

	if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_BEGIN)) {
		set_dom_string(&str, value->string, selvalue->length);

		return !dom_string_casecmp(&str, selvalue);
	}

	if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_END)) {
		size_t offset = value->length - selvalue->length;

		set_dom_string(&str, value->string + offset, selvalue->length);

		return !dom_string_casecmp(&str, selvalue);
	}

	/* The 3 following matching methods requires the selector value to be a
	 * substring of the value enclosed in a specific separator (with the
	 * begining and ending of the attribute value both being valid
	 * separators). */

	set_dom_string(&str, value->string, value->length);

	if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_HYPHEN_LIST)) {
		separator = '-';

	} else if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_CONTAINS)) {
		separator = '\0';

	} if (has_attribute_match(selector, DOM_SELECT_ATTRIBUTE_SPACE_LIST)) {
		separator = ' ';

	} else {
		INTERNAL("No attribute selector matching method defined");
		return 0;
	}

	do_compare = 1;

	do {
		if (do_compare
		    && !dom_string_ncasecmp(&str, selvalue, selvalue->length)) {
			/* "Contains" matches no matter what comes after. */
			if (str.length == selvalue->length)
				return 1;

			switch (separator) {
			case '\0':
				/* "Contains" matches no matter what comes after. */
				return 1;

			case '-':
				if (str.string[str.length] == separator)
					return 1;
				break;

			default:
				if (isspace(str.string[str.length]))
					return 1;
			}
		}

		switch (separator) {
		case '\0':
			do_compare = 1;
			break;

		case '-':
			do_compare = (str.string[0] == '-');
			break;

		default:
			do_compare = isspace(str.string[0]);
		}

		str.length--, str.string++;

	} while (str.length >= selvalue->length);

	return 0;
}

/* Match the attribute of an element @node against attribute selector nodes
 * of a given @base. */
static int
match_attribute_selectors(struct dom_select_node *base, struct dom_node *node)
{
	struct dom_node_list *attrs = node->data.element.map;
	struct dom_node_list *selnodes = base->node.data.element.map;
	struct dom_node *selnode;
	size_t index;

	assert(base->node.type == DOM_NODE_ELEMENT
	       && node->type == DOM_NODE_ELEMENT);

	/* If there are no attribute selectors that is a clean match ... */
	if (!selnodes)
		return 1;

	/* ... the opposite goes if there are no attributes to match. */
	if (!attrs)
		return 0;

	foreach_dom_node (selnodes, selnode, index) {
		struct dom_select_node *selector = (void *) selnode;
		struct dom_node *attr;

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

		if (!match_attribute_value(selector, attr))
			return 0;
	}

	return 1;
}

/* XXX: Assume the first context is the one! */
#define get_dom_select_state(stack, state) \
	((struct dom_select_state *) get_dom_stack_state_data((stack)->contexts[0], state))

static int
match_element_relation(struct dom_select_node *selector, struct dom_node *node,
		       struct dom_stack *stack)
{
	struct dom_stack_state *state;
	enum dom_select_element_match relation = get_element_relation(selector);
	int i, index;

	assert(relation);

	/* When matching any relation there must be a parent, either so that
	 * the node is a descendant or it is possible to check for siblings. */
	if (!node->parent)
		return 0;

	if (relation != DOM_SELECT_RELATION_DIRECT_CHILD) {
		/* When looking for preceeding siblings of the current node,
		 * the current node cannot be first or not in the list (-1). */
		index = get_dom_node_list_index(node->parent, node);
		if (index < 1)
			return 0;
	} else {
		index = -1;
	}

	/* Find states which hold the parent of the current selector
	 * and check if the parent selector's node is the parent of the
	 * current node. */
	foreachback_dom_stack_state(stack, state, i) {
		struct dom_node *selnode;

		/* We are only interested in states which hold the parent of
		 * the current selector. */
		if (state->node != selector->node.parent)
			continue;

		selnode = get_dom_select_state(stack, state)->node;

		if (relation == DOM_SELECT_RELATION_DIRECT_CHILD) {
			/* Check if the parent selector's node is the parent of
			 * the current node. */
			if (selnode == node->parent)
				return 1;

		} else {
			int sibindex;

			/* Check if they are siblings. */
			if (selnode->parent != node->parent)
				continue;

			sibindex = get_dom_node_list_index(node->parent, selnode);

			if (relation == DOM_SELECT_RELATION_DIRECT_ADJACENT) {
				/* Check if the sibling node immediately
				 * preceeds the current node. */
				if (sibindex + 1 == index)
					return 1;

			} else { /* DOM_SELECT_RELATION_INDIRECT_ADJACENT */
				/* Check if the sibling node preceeds the
				 * current node. */
				if (sibindex < index)
					return 1;
			}
		}
	}

	return 0;
}

#define has_element_match(selector, name) \
	((selector)->match.element & (name))

static int
match_element_selector(struct dom_select_node *selector, struct dom_node *node,
		       struct dom_stack *stack)
{
	assert(node && node->type == DOM_NODE_ELEMENT);

	if (!has_element_match(selector, DOM_SELECT_ELEMENT_UNIVERSAL)
	    && dom_node_casecmp(&selector->node, node))
		return 0;

	if (get_element_relation(selector) != DOM_SELECT_RELATION_DESCENDANT
	    && !match_element_relation(selector, node, stack))
		return 0;

	/* Root nodes either have no parents or are the single child of the
	 * document node. */
	if (has_element_match(selector, DOM_SELECT_ELEMENT_ROOT)
	    && node->parent) {
		if (node->parent->type != DOM_NODE_DOCUMENT
		    || node->parent->data.document.children->size > 1)
			return 0;
	}

	if (has_element_match(selector, DOM_SELECT_ELEMENT_EMPTY)
	    && node->data.element.children
	    && node->data.element.children->size > 0)
		return 0;

	if (has_element_match(selector, DOM_SELECT_ELEMENT_NTH_CHILD)) {
		/* FIXME */
		return 0;
	}

	if (has_element_match(selector, DOM_SELECT_ELEMENT_NTH_TYPE)) {
		/* FIXME */
		return 0;
	}

	/* Check attribute selectors. */
	if (selector->node.data.element.map
	    && !match_attribute_selectors(selector, node))
		return 0;

	return 1;
}


#define get_dom_select_data(stack) ((stack)->current->data)

/* Matches an element node being visited against the current selector stack. */
enum dom_code
dom_select_push_element(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_select_data *select_data = get_dom_select_data(stack);
	struct dom_stack_state *state;
	int pos;

	foreach_dom_stack_state(&select_data->stack, state, pos) {
		struct dom_select_node *selector = (void *) state->node;

		/* FIXME: Since the same dom_select_node can be multiple times
		 * on the select_data->stack, cache what select nodes was
		 * matches so that it is only checked once. */

		if (!match_element_selector(selector, node, &select_data->stack))
			continue;

		WDBG("Matched element: %.*s.", node->string.length, node->string.string);
		/* This node is matched, so push the next selector node to
		 * match on the stack. */
		selector = get_child_dom_select_node(selector, DOM_NODE_ELEMENT);
		if (selector)
			push_dom_node(&select_data->stack, &selector->node);
	}

	return DOM_CODE_OK;
}

/* Ensures that nodes, no longer 'reachable' on the stack do not have any
 * states associated with them on the select data stack. */
enum dom_code
dom_select_pop_element(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_select_data *select_data = get_dom_select_data(stack);
	struct dom_stack_state *state;
	int index;

	stack = &select_data->stack;

	foreachback_dom_stack_state (stack, state, index) {
		struct dom_select_state *select_state;

		select_state = get_dom_select_state(stack, state);
		if (select_state->node == node) {
			pop_dom_state(stack, state);
			WDBG("Remove element.");
			continue;
		}
	}

	return DOM_CODE_OK;
}

/* For now this is only for matching the ':contains(<string>)' pseudo-class.
 * Any node which can contain text and thus characters from the given <string>
 * are handled in this common callback. */
enum dom_code
dom_select_push_text(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_select_data *select_data = get_dom_select_data(stack);
	struct dom_stack_state *state = get_dom_stack_top(&select_data->stack);
	struct dom_select_node *selector = (void *) state->node;
	struct dom_select_node *text_sel = get_child_dom_select_node(selector, DOM_NODE_TEXT);
	struct dom_string *text;

	WDBG("Text node: %d chars", node->string.length);

	if (!text_sel)
		return DOM_CODE_OK;

	text = &text_sel->node.string;

	switch (node->type) {
	case DOM_NODE_TEXT:
	case DOM_NODE_CDATA_SECTION:
	case DOM_NODE_ENTITY_REFERENCE:
		break;
	default:
		ERROR("Unhandled type");
	}

	return DOM_CODE_OK;
}

/* Context info for interacting with the DOM tree or stream stack. */
static struct dom_stack_context_info dom_select_context_info = {
	/* Object size: */			0,
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

/* Context info related to the private select data stack of matched nodes. */
static struct dom_stack_context_info dom_select_data_context_info = {
	/* Object size: */			sizeof(struct dom_select_state),
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


struct dom_node_list *
select_dom_nodes(struct dom_select *select, struct dom_node *root)
{
	struct dom_select_data select_data;
	struct dom_stack stack;

	memset(&select_data, 0, sizeof(select_data));

	select_data.select = select;;

	init_dom_stack(&stack, DOM_STACK_FLAG_NONE);
	add_dom_stack_context(&stack, &select_data,
			      &dom_select_context_info);
	add_dom_stack_tracer(&stack, "select-tree: ");

	init_dom_stack(&select_data.stack, DOM_STACK_FLAG_NONE);
	add_dom_stack_context(&select_data.stack, &select_data,
			      &dom_select_data_context_info);
	add_dom_stack_tracer(&select_data.stack, "select-match: ");

	if (push_dom_node(&select_data.stack, &select->selector->node) == DOM_CODE_OK) {
		get_dom_stack_top(&select_data.stack)->immutable = 1;
		walk_dom_nodes(&stack, root);
	}

	done_dom_stack(&select_data.stack);
	done_dom_stack(&stack);

	return select_data.list;
}
