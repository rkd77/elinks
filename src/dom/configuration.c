/* DOM Configuration */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "dom/configuration.h"
#include "dom/node.h"
#include "dom/stack.h"
#include "dom/string.h"


static enum dom_code
normalize_text_node_whitespace(struct dom_node *node)
{
	unsigned char buf[256];
	struct dom_string string = INIT_DOM_STRING(NULL, 0);
	int count = 0, i = 0;
	unsigned char *text = node->string.string;

	assert(node->type == DOM_NODE_TEXT);

	while (i < node->string.length) {
		int j;

		for (j = 0; j < sizeof(buf) && i < node->string.length; i++) {
			unsigned char data = text[i];

			if (isspace(data)) {
				if (count == 1)
					continue;

				data = ' ';
				count = 1;

			} else {
				count = 0;
			}

			buf[j++] = data;
		}

		if (!add_to_dom_string(&string, buf, j)) {
			done_dom_string(&string);
			return DOM_CODE_ALLOC_ERR;
		}
	}

	if (node->allocated)
		done_dom_string(&node->string);

	set_dom_string(&node->string, string.string, string.length);
	node->allocated = 1;

	return DOM_CODE_OK;

}

static enum dom_code
append_node_text(struct dom_config *config, struct dom_node *node)
{
	struct dom_node *prev = get_dom_node_prev(node);
	size_t length;
	struct dom_string dest;
	struct dom_string src;
	int error = 0;

	copy_struct(&src, &node->string);

	if (!prev || prev->type != DOM_NODE_TEXT) {
		/* Preserve text nodes with no one to append to. */
		if (node->type == DOM_NODE_TEXT)
			return DOM_CODE_OK;

		prev = NULL;
		set_dom_string(&dest, NULL, 0);

	} else {
		if (prev->allocated) {
			copy_struct(&dest, &prev->string);
		} else {
			set_dom_string(&dest, NULL, 0);
			if (!add_to_dom_string(&dest, prev->string.string, prev->string.length))
				return DOM_CODE_ALLOC_ERR;
			set_dom_string(&prev->string, dest.string, dest.length);
			prev->allocated = 1;
		}
	}

	length = dest.length;

	switch (node->type) {
	case DOM_NODE_CDATA_SECTION:
	case DOM_NODE_TEXT:
		if (!add_to_dom_string(&dest, src.string, src.length))
			error = 1;
		break;

	case DOM_NODE_ENTITY_REFERENCE:
		/* FIXME: Until we will have uniform encoding at this point
		 * (UTF-8) we just add the entity reference unexpanded assuming
		 * that convert_string() will eventually do the work of
		 * expanding it. */
		if (!add_to_dom_string(&dest, "&", 1)
		    || !add_to_dom_string(&dest, src.string, src.length)
		    || !add_to_dom_string(&dest, ";", 1)) {
			error = 1;
		}
		break;

	default:
		INTERNAL("Cannot append from node %d", node->type);
	}

	if (error) {
		if (prev)
			prev->string.length = length;
		else
			done_dom_string(&dest);
		return DOM_CODE_ALLOC_ERR;
	}

	if (prev) {
		copy_struct(&prev->string, &dest);

		if ((config->flags & DOM_CONFIG_NORMALIZE_WHITESPACE)
		    && node->type != DOM_NODE_ENTITY_REFERENCE) {
			/* XXX: Ignore errors since we want to always
			 * free the appended node at this point. */
			normalize_text_node_whitespace(prev);
		}

		return DOM_CODE_FREE_NODE;

	} else {
		int was_cdata_section = node->type == DOM_NODE_CDATA_SECTION;

		node->type = DOM_NODE_TEXT;
		memset(&node->data, 0, sizeof(node->data));
		node->allocated = 1;
		copy_struct(&node->string, &dest);

		if ((config->flags & DOM_CONFIG_NORMALIZE_WHITESPACE)
		    && was_cdata_section) {
			/* XXX: Ignore errors since we want to always ok the
			 * append. */
			normalize_text_node_whitespace(node);
		}

		return DOM_CODE_OK;
	}
}

static enum dom_code
dom_normalize_node_end(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_config *config = stack->current->data;
	enum dom_code code = DOM_CODE_OK;

	switch (node->type) {
	case DOM_NODE_ELEMENT:
		if ((config->flags & DOM_CONFIG_UNKNOWN)
		    && !node->data.element.type) {
			/* Drop elements that are not known from the built-in
			 * node info. */
			code = DOM_CODE_FREE_NODE;
		}
		break;

	case DOM_NODE_ATTRIBUTE:
		if ((config->flags & DOM_CONFIG_UNKNOWN)
		    && !node->data.attribute.type) {
			/* Drop elements that are not known from the built-in
			 * node info. */
			code = DOM_CODE_FREE_NODE;
		}
		break;

	case DOM_NODE_PROCESSING_INSTRUCTION:
		if ((config->flags & DOM_CONFIG_UNKNOWN)
		    && !node->data.proc_instruction.type) {
			/* Drop elements that are not known from the built-in
			 * node info. */
			code = DOM_CODE_FREE_NODE;
		}
		break;

	case DOM_NODE_TEXT:
		if (!(config->flags & DOM_CONFIG_ELEMENT_CONTENT_WHITESPACE)
		    && node->data.text.only_space) {
			/* Discard all Text nodes that contain
			 * whitespaces in element content]. */
			code = DOM_CODE_FREE_NODE;
		} else {
			code = append_node_text(config, node);
		}
		break;

	case DOM_NODE_COMMENT:
		if (!(config->flags & DOM_CONFIG_COMMENTS)) {
			/* Discard all comments. */
			code = DOM_CODE_FREE_NODE;
		}
		break;

	case DOM_NODE_CDATA_SECTION:
		if (!(config->flags & DOM_CONFIG_CDATA_SECTIONS)) {
			/* Transform CDATASection nodes into Text nodes. The new Text
			 * node is then combined with any adjacent Text node. */
			code = append_node_text(config, node);
		}
		break;

	case DOM_NODE_ENTITY_REFERENCE:
		if (!(config->flags & DOM_CONFIG_ENTITIES)) {
			/* Remove all EntityReference nodes from the document,
			 * putting the entity expansions directly in their place. Text
			 * nodes are normalized. Only unexpanded entity references are
			 * kept in the document. */
			code = append_node_text(config, node);
		}
		break;

	case DOM_NODE_DOCUMENT:
		break;

	default:
		break;
	}

	return code;
}

enum dom_code
dom_normalize_text(struct dom_stack *stack, struct dom_node *node, void *data)
{
	struct dom_config *config = stack->current->data;

	if (config->flags & DOM_CONFIG_NORMALIZE_WHITESPACE) {
		/* Normalize whitespace in the text. */
		return normalize_text_node_whitespace(node);
	}

	return DOM_CODE_OK;
}


static struct dom_stack_context_info dom_config_normalizer_context = {
	/* Object size: */			0,
	/* Push: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ NULL,
		/* DOM_NODE_ATTRIBUTE		*/ NULL,
		/* DOM_NODE_TEXT		*/ dom_normalize_text,
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
		/* DOM_NODE_ELEMENT		*/ dom_normalize_node_end,
		/* DOM_NODE_ATTRIBUTE		*/ dom_normalize_node_end,
		/* DOM_NODE_TEXT		*/ dom_normalize_node_end,
		/* DOM_NODE_CDATA_SECTION	*/ dom_normalize_node_end,
		/* DOM_NODE_ENTITY_REFERENCE	*/ dom_normalize_node_end,
		/* DOM_NODE_ENTITY		*/ dom_normalize_node_end,
		/* DOM_NODE_PROC_INSTRUCTION	*/ dom_normalize_node_end,
		/* DOM_NODE_COMMENT		*/ dom_normalize_node_end,
		/* DOM_NODE_DOCUMENT		*/ dom_normalize_node_end,
		/* DOM_NODE_DOCUMENT_TYPE	*/ dom_normalize_node_end,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ dom_normalize_node_end,
		/* DOM_NODE_NOTATION		*/ dom_normalize_node_end,
	}
};

struct dom_config *
add_dom_config_normalizer(struct dom_stack *stack, struct dom_config *config, 
			  enum dom_config_flag flags)
{
	memset(config, 0, sizeof(*config));
	config->flags = flags;

	if (add_dom_stack_context(stack, config, &dom_config_normalizer_context))
		return config;

	return NULL;
}

struct dom_config_info {
	struct dom_string name;
	enum dom_config_flag flag;
};

#define DOM_CONFIG(name, flag) \
	{ STATIC_DOM_STRING(name), (flag) }

static struct dom_config_info dom_config_info[] = {
	DOM_CONFIG("cdata-sections",		DOM_CONFIG_CDATA_SECTIONS),
	DOM_CONFIG("comments",			DOM_CONFIG_COMMENTS),
	DOM_CONFIG("element-content-whitespace",DOM_CONFIG_ELEMENT_CONTENT_WHITESPACE),
	DOM_CONFIG("entities",			DOM_CONFIG_ENTITIES),
	DOM_CONFIG("normalize-characters",	DOM_CONFIG_NORMALIZE_CHARACTERS),
	DOM_CONFIG("unknown",			DOM_CONFIG_UNKNOWN),
	DOM_CONFIG("normalize-whitespace",	DOM_CONFIG_NORMALIZE_WHITESPACE),
};

static enum dom_config_flag
get_dom_config_flag(struct dom_string *name)
{
	int i;

	for (i = 0; i < sizeof_array(dom_config_info); i++)
		if (!dom_string_casecmp(&dom_config_info[i].name, name))
			return dom_config_info[i].flag;

	return 0;
}

enum dom_config_flag
parse_dom_config(unsigned char *flaglist, unsigned char separator)
{
	enum dom_config_flag flags = 0;

	while (flaglist) {
		unsigned char *end = separator ? strchr(flaglist, separator) : NULL;
		int length = end ? end - flaglist : strlen(flaglist);
		struct dom_string name = INIT_DOM_STRING(flaglist, length);

		flags |= get_dom_config_flag(&name);
		if (end) end++;
		flaglist = end;
	}

	return flags;
}
