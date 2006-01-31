/* SGML generics */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "dom/node.h"
#include "dom/sgml/dump.h"
#include "dom/stack.h"
#include "dom/string.h"


static enum dom_code
sgml_file_dumper_element_push(struct dom_stack *stack, struct dom_node *node, void *data)
{
	FILE *file = stack->current->data;
	struct dom_string *string = &node->string;

	fprintf(file, "<%.*s", string->length, string->string);
	if (!node->data.element.map
	    || node->data.element.map->size == 0)
		fprintf(file, ">");

	return DOM_CODE_OK;
}

static enum dom_code
sgml_file_dumper_element_pop(struct dom_stack *stack, struct dom_node *node, void *data)
{
	FILE *file = stack->current->data;
	struct dom_string *string = &node->string;

	fprintf(file, "</%.*s>", string->length, string->string);

	return DOM_CODE_OK;
}

static enum dom_code
sgml_file_dumper_attribute_push(struct dom_stack *stack, struct dom_node *node, void *data)
{
	FILE *file = stack->current->data;
	struct dom_string *name = &node->string;
	struct dom_string *value = &node->data.attribute.value;

	if (node->parent->type == DOM_NODE_PROCESSING_INSTRUCTION)
		return DOM_CODE_OK;

	fprintf(file, " %.*s", name->length, name->string);

	if (value->string) {
		if (node->data.attribute.quoted)
			fprintf(file, "=%c%.*s%c",
				node->data.attribute.quoted,
				value->length, value->string,
				node->data.attribute.quoted);
		else
			/* FIXME: 'encode' if the value contains '"'s? */
			fprintf(file, "=\"%.*s\"", value->length, value->string);
	}

	if (!get_dom_node_next(node))
		fprintf(file, ">");

	return DOM_CODE_OK;
}

static enum dom_code
sgml_file_dumper_proc_instruction_push(struct dom_stack *stack, struct dom_node *node, void *data)
{
	FILE *file = stack->current->data;
	struct dom_string *target = &node->string;
	struct dom_string *instruction = &node->data.proc_instruction.instruction;

	fprintf(file, "<?%.*s %.*s?>",
		target->length, target->string,
		instruction->length, instruction->string);

	return DOM_CODE_OK;
}

static enum dom_code
sgml_file_dumper_text_push(struct dom_stack *stack, struct dom_node *node, void *data)
{
	FILE *file = stack->current->data;
	struct dom_string *string = &node->string;

	fprintf(file, "%.*s", string->length, string->string);

	return DOM_CODE_OK;
}

static enum dom_code
sgml_file_dumper_cdata_section_push(struct dom_stack *stack, struct dom_node *node, void *data)
{
	FILE *file = stack->current->data;
	struct dom_string *string = &node->string;

	fprintf(file, "<![CDATA[%.*s]]>", string->length, string->string);

	return DOM_CODE_OK;
}

static enum dom_code
sgml_file_dumper_comment_push(struct dom_stack *stack, struct dom_node *node, void *data)
{
	FILE *file = stack->current->data;
	struct dom_string *string = &node->string;

	fprintf(file, "<!--%.*s-->", string->length, string->string);

	return DOM_CODE_OK;
}

static enum dom_code
sgml_file_dumper_entity_ref_push(struct dom_stack *stack, struct dom_node *node, void *data)
{
	FILE *file = stack->current->data;
	struct dom_string *string = &node->string;

	fprintf(file, "&%.*s;", string->length, string->string);

	return DOM_CODE_OK;
}

struct dom_stack_context_info sgml_file_dumper = {
	/* Object size: */			0,
	/* Push: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ sgml_file_dumper_element_push,
		/* DOM_NODE_ATTRIBUTE		*/ sgml_file_dumper_attribute_push,
		/* DOM_NODE_TEXT		*/ sgml_file_dumper_text_push,
		/* DOM_NODE_CDATA_SECTION	*/ sgml_file_dumper_cdata_section_push,
		/* DOM_NODE_ENTITY_REFERENCE	*/ sgml_file_dumper_entity_ref_push,
		/* DOM_NODE_ENTITY		*/ NULL,
		/* DOM_NODE_PROC_INSTRUCTION	*/ sgml_file_dumper_proc_instruction_push,
		/* DOM_NODE_COMMENT		*/ sgml_file_dumper_comment_push,
		/* DOM_NODE_DOCUMENT		*/ NULL,
		/* DOM_NODE_DOCUMENT_TYPE	*/ NULL,
		/* DOM_NODE_DOCUMENT_FRAGMENT	*/ NULL,
		/* DOM_NODE_NOTATION		*/ NULL,
	},
	/* Pop: */
	{
		/*				*/ NULL,
		/* DOM_NODE_ELEMENT		*/ sgml_file_dumper_element_pop,
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

struct dom_stack_context *
add_sgml_file_dumper(struct dom_stack *stack, FILE *file)
{
	return add_dom_stack_context(stack, file, &sgml_file_dumper);
}
