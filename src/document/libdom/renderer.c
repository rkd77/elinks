/* Plain text document renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bookmarks/bookmarks.h"
#include "cache/cache.h"
#include "config/options.h"
#include "document/docdata.h"
#include "document/document.h"
#include "document/format.h"
#include "document/options.h"
#include "document/libdom/renderer.h"
#include "document/plain/renderer.h"
#include "document/renderer.h"
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>

struct source_renderer {
	struct string tmp_buffer;
	struct string *source;
	char *enc;
};

/**
 * Generate a LibDOM document DOM from an HTML file
 *
 * \param file  The file path
 * \return  pointer to DOM document, or NULL on error
 */
static dom_document *
create_doc_dom_from_buffer(struct source_renderer *renderer)
{
	dom_hubbub_parser *parser = NULL;
	dom_hubbub_error error;
	dom_hubbub_parser_params params;
	dom_document *doc;

	params.enc = renderer->enc;
	params.fix_enc = true;
	params.enable_script = false;
	params.msg = NULL;
	params.script = NULL;
	params.ctx = NULL;
	params.daf = NULL;

	/* Create Hubbub parser */
	error = dom_hubbub_parser_create(&params, &parser, &doc);
	if (error != DOM_HUBBUB_OK) {
		DBG("Can't create Hubbub Parser\n");
		return NULL;
	}

	/* Parse data */
	error = dom_hubbub_parser_parse_chunk(parser, renderer->source->source, renderer->source->length);
	if (error != DOM_HUBBUB_OK) {
		dom_hubbub_parser_destroy(parser);
		DBG("Parsing errors occur %d\n", error & ~DOM_HUBBUB_HUBBUB_ERR);
		return NULL;
	}

	/* Done parsing file */
	error = dom_hubbub_parser_completed(parser);
	if (error != DOM_HUBBUB_OK) {
		dom_hubbub_parser_destroy(parser);
		DBG("Parsing error when construct DOM\n");
		return NULL;
	}

	/* Finished with parser */
	dom_hubbub_parser_destroy(parser);

	return doc;
}

/**
 * Dump attribute/value for an element node
 *
 * \param node       The attribute node to dump details for
 * \return  true on success, or false on error
 */
static bool
dump_node_element_attribute(struct source_renderer *renderer, dom_node *node)
{
	dom_exception exc;
	dom_string *attr = NULL;
	dom_string *attr_value = NULL;

	exc = dom_attr_get_name((struct dom_attr *)node, &attr);

	if (exc != DOM_NO_ERR) {
		DBG("Exception raised for dom_string_create\n");
		return false;
	}

	/* Get attribute's value */
	exc = dom_attr_get_value((struct dom_attr *)node, &attr_value);
	if (exc != DOM_NO_ERR) {
		DBG("Exception raised for element_get_attribute\n");
		dom_string_unref(attr);
		return false;
	} else if (attr_value == NULL) {
		/* Element lacks required attribute */
		dom_string_unref(attr);
		return true;
	}

	add_to_string(&renderer->tmp_buffer, " \033[0;33m");
	add_bytes_to_string(&renderer->tmp_buffer, dom_string_data(attr), dom_string_byte_length(attr));
	add_to_string(&renderer->tmp_buffer, "\033[0m=\"\033[0;35m");
	add_bytes_to_string(&renderer->tmp_buffer, dom_string_data(attr_value), dom_string_byte_length(attr_value));
	add_to_string(&renderer->tmp_buffer, "\033[0m\"");

	/* Finished with the attr dom_string */
	dom_string_unref(attr);
	dom_string_unref(attr_value);

	return true;
}

/**
 * Print a line in a DOM structure dump for an element
 *
 * \param node   The node to dump
 * \param depth  The node's depth
 * \return  true on success, or false on error
 */
static bool
dump_dom_element(struct source_renderer *renderer, dom_node *node, int depth)
{
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_node_type type;
	dom_namednodemap *attrs;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &type);

	if (exc != DOM_NO_ERR) {
		DBG("Exception raised for node_get_node_type\n");
		return false;
	} else {
		if (type == DOM_TEXT_NODE) {
			dom_string *str;

			exc = dom_node_get_text_content(node, &str);

			if (exc == DOM_NO_ERR && str != NULL) {
				int length = dom_string_byte_length(str);
				const char *string = dom_string_data(str);

				if (!((length == 1) && (*string == '\n'))) {
					add_bytes_to_string(&renderer->tmp_buffer, string, length);
				}
				dom_string_unref(str);
			}
			return true;
		}
		if (type != DOM_ELEMENT_NODE) {
			/* Nothing to print */
			return true;
		}
	}

	/* Get element name */
	exc = dom_node_get_node_name(node, &node_name);
	if (exc != DOM_NO_ERR) {
		DBG("Exception raised for get_node_name\n");
		return false;
	} else if (node_name == NULL) {
		DBG("Broken: root_name == NULL\n");
 		return false;
	}

	/* Get string data and print element name */
	add_to_string(&renderer->tmp_buffer, "<\033[0;32m");
	add_bytes_to_string(&renderer->tmp_buffer, dom_string_data(node_name), dom_string_byte_length(node_name));
	add_to_string(&renderer->tmp_buffer, "\033[0m");

	exc = dom_node_get_attributes(node, &attrs);

	if (exc == DOM_NO_ERR) {
		int length;

		exc = dom_namednodemap_get_length(attrs, &length);

		if (exc == DOM_NO_ERR) {
			int i;

			for (i = 0; i < length; ++i) {
				dom_node *attr;

				exc = dom_namednodemap_item(attrs, i, &attr);

				if (exc == DOM_NO_ERR) {
					dump_node_element_attribute(renderer, attr);
					dom_node_unref(attr);
				}
			}
		}
		dom_node_unref(attrs);
	}
	add_char_to_string(&renderer->tmp_buffer, '>');

	/* Finished with the node_name dom_string */
	dom_string_unref(node_name);

	return true;
}

/**
 * Print a closing element
 *
 * \param node   The node to dump
 * \return  true on success, or false on error
 */
static bool
dump_dom_element_closing(struct source_renderer *renderer, dom_node *node)
{
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_node_type type;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &type);

	if (exc != DOM_NO_ERR) {
		DBG("Exception raised for node_get_node_type\n");
		return false;
	} else { 
		if (type != DOM_ELEMENT_NODE) {
			/* Nothing to print */
			return true;
		}
	}

	/* Get element name */
	exc = dom_node_get_node_name(node, &node_name);
	if (exc != DOM_NO_ERR) {
		DBG("Exception raised for get_node_name\n");
		return false;
	} else if (node_name == NULL) {
		DBG("Broken: root_name == NULL\n");
 		return false;
	}

	/* Get string data and print element name */
	add_to_string(&renderer->tmp_buffer, "</\033[0;32m");
	add_bytes_to_string(&renderer->tmp_buffer, dom_string_data(node_name), dom_string_byte_length(node_name));
	add_to_string(&renderer->tmp_buffer, "\033[0m>");

	/* Finished with the node_name dom_string */
	dom_string_unref(node_name);

	return true;
}


/**
 * Walk though a DOM (sub)tree, in depth first order, printing DOM structure.
 *
 * \param node   The root node to start from
 * \param depth  The depth of 'node' in the (sub)tree
 */
static bool
dump_dom_structure(struct source_renderer *renderer, dom_node *node, int depth)
{
	dom_exception exc;
	dom_node *child;

	/* Print this node's entry */
	if (dump_dom_element(renderer, node, depth) == false) {
		/* There was an error; return */
		return false;
	}

	/* Get the node's first child */
	exc = dom_node_get_first_child(node, &child);
	if (exc != DOM_NO_ERR) {
		DBG("Exception raised for node_get_first_child\n");
		return false;
	} else if (child != NULL) {
		/* node has children;  decend to children's depth */
		depth++;

		/* Loop though all node's children */
		do {
			dom_node *next_child;

			/* Visit node's descendents */
			if (dump_dom_structure(renderer, child, depth) == false) {
				/* There was an error; return */
				dom_node_unref(child);
				return false;
			}

			/* Go to next sibling */
			exc = dom_node_get_next_sibling(child, &next_child);
			if (exc != DOM_NO_ERR) {
				DBG("Exception raised for "
						"node_get_next_sibling\n");
				dom_node_unref(child);
				return false;
			}

			dom_node_unref(child);
			child = next_child;
		} while (child != NULL); /* No more children */
	}

	dump_dom_element_closing(renderer, node);

	return true;
}

/**
 * Main entry point from OS.
 */
static int
libdom_main(struct source_renderer *renderer)
{
	dom_exception exc; /* returned by libdom functions */
	dom_document *doc = NULL; /* document, loaded into libdom */
	dom_node *root = NULL; /* root element of document */

	/* Load up the input HTML file */
	doc = create_doc_dom_from_buffer(renderer);
	if (doc == NULL) {
		DBG("Failed to load document.\n");
		return EXIT_FAILURE;
	}

	/* Get root element */
	exc = dom_document_get_document_element(doc, &root);
	if (exc != DOM_NO_ERR) {
		DBG("Exception raised for get_document_element\n");
		dom_node_unref(doc);
 		return EXIT_FAILURE;
	} else if (root == NULL) {
		DBG("Broken: root == NULL\n");
		dom_node_unref(doc);
 		return EXIT_FAILURE;
	}

	/* Dump DOM structure */
	if (dump_dom_structure(renderer, root, 0) == false) {
		DBG("Failed to complete DOM structure dump.\n");
		dom_node_unref(root);
		dom_node_unref(doc);
		return EXIT_FAILURE;
	}

	dom_node_unref(root);

	/* Finished with the dom_document */
	dom_node_unref(doc);

	return EXIT_SUCCESS;
}

void
render_source_document(struct cache_entry *cached, struct document *document,
		      struct string *buffer)
{
	struct source_renderer renderer;
	unsigned char *head = empty_string_or_(cached->head);

	(void)get_convert_table(head, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

	init_string(&renderer.tmp_buffer);
	renderer.source = buffer;
	renderer.enc = get_cp_mime_name(document->cp);
	libdom_main(&renderer);
	render_plain_document(cached, document, &renderer.tmp_buffer);
	done_string(&renderer.tmp_buffer);
}
