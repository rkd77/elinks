/* libdom to text document renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#include <stdio.h>

#include "elinks.h"

#include "cache/cache.h"
#include "document/document.h"
#include "document/renderer.h"
#include "document/libdom/doc.h"
#include "document/libdom/mapa.h"
#include "document/libdom/renderer.h"
#include "document/plain/renderer.h"
#include "ecmascript/libdom/parse.h"

static bool
dump_dom_element_closing(struct string *buf, dom_node *node)
{
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_node_type type;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &type);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for node_get_node_type\n");
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
		fprintf(stderr, "Exception raised for get_node_name\n");
		return false;
	} else if (node_name == NULL) {
		fprintf(stderr, "Broken: root_name == NULL\n");
		return false;
	}

	/* Get string data and print element name */
	add_to_string(buf, "</");
	add_bytes_to_string(buf, dom_string_data(node_name), dom_string_byte_length(node_name));
	add_char_to_string(buf, '>');

	/* Finished with the node_name dom_string */
	dom_string_unref(node_name);

	return true;
}

static bool
dump_node_element_attribute(struct string *buf, dom_node *node)
{
	dom_exception exc;
	dom_string *attr = NULL;
	dom_string *attr_value = NULL;

	exc = dom_attr_get_name((struct dom_attr *)node, &attr);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for dom_string_create\n");
		return false;
	}

	/* Get attribute's value */
	exc = dom_attr_get_value((struct dom_attr *)node, &attr_value);
	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for element_get_attribute\n");
		dom_string_unref(attr);
		return false;
	} else if (attr_value == NULL) {
		/* Element lacks required attribute */
		dom_string_unref(attr);
		return true;
	}

	add_char_to_string(buf, ' ');
	add_bytes_to_string(buf, dom_string_data(attr), dom_string_byte_length(attr));
	add_to_string(buf, "=\"");
	add_bytes_to_string(buf, dom_string_data(attr_value), dom_string_byte_length(attr_value));
	add_char_to_string(buf, '"');

	/* Finished with the attr dom_string */
	dom_string_unref(attr);
	dom_string_unref(attr_value);

	return true;
}


static bool
dump_dom_element(void *mapa, struct string *buf, dom_node *node, int depth)
{
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_node_type type;
	dom_namednodemap *attrs;

	/* Only interested in element nodes */
	exc = dom_node_get_node_type(node, &type);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for node_get_node_type\n");
		return false;
	} else {
		if (type == DOM_TEXT_NODE) {
			dom_string *str;

			exc = dom_node_get_text_content(node, &str);

			if (exc == DOM_NO_ERR && str != NULL) {
				int length = dom_string_byte_length(str);
				const char *string_text = dom_string_data(str);

				add_bytes_to_string(buf, string_text, length);
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
		fprintf(stderr, "Exception raised for get_node_name\n");
		return false;
	}

	add_char_to_string(buf, '<');
	save_in_map(mapa, node, buf->length);

	/* Get string data and print element name */
	add_bytes_to_string(buf, dom_string_data(node_name), dom_string_byte_length(node_name));

	exc = dom_node_get_attributes(node, &attrs);

	if (exc == DOM_NO_ERR) {
		dom_ulong length;

		exc = dom_namednodemap_get_length(attrs, &length);

		if (exc == DOM_NO_ERR) {
			int i;

			for (i = 0; i < length; ++i) {
				dom_node *attr;

				exc = dom_namednodemap_item(attrs, i, &attr);

				if (exc == DOM_NO_ERR) {
					dump_node_element_attribute(buf, attr);
					dom_node_unref(attr);
				}
			}
		}
		dom_node_unref(attrs);
	}
	add_char_to_string(buf, '>');

	/* Finished with the node_name dom_string */
	dom_string_unref(node_name);

	return true;
}

static bool
walk_tree(void *mapa, struct string *buf, dom_node *node, bool start, int depth)
{
	dom_exception exc;
	dom_node *child;

	/* Print this node's entry */
	if (dump_dom_element(mapa, buf, node, depth) == false) {
		/* There was an error; return */
		return false;
	}

	/* Get the node's first child */
	exc = dom_node_get_first_child(node, &child);
	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for node_get_first_child\n");
		return false;
	} else if (child != NULL) {
		/* node has children;  decend to children's depth */
		depth++;

		/* Loop though all node's children */
		do {
			dom_node *next_child;

			/* Visit node's descendents */
			if (walk_tree(mapa, buf, child, false, depth) == false) {
				/* There was an error; return */
				dom_node_unref(child);
				return false;
			}

			/* Go to next sibling */
			exc = dom_node_get_next_sibling(child, &next_child);
			if (exc != DOM_NO_ERR) {
				fprintf(stderr, "Exception raised for "
						"node_get_next_sibling\n");
				dom_node_unref(child);
				return false;
			}

			dom_node_unref(child);
			child = next_child;
		} while (child != NULL); /* No more children */
	}
	dump_dom_element_closing(buf, node);

	return true;
}

void
render_source_document_cxx(struct cache_entry *cached, struct document *document, struct string *buffer)
{
	dom_exception exc; /* returned by libdom functions */
	dom_document *doc = NULL; /* document, loaded into libdom */
	dom_node *root = NULL; /* root element of document */
	void *mapa = NULL;

	if (!document->dom) {
	(void)get_convert_table(cached->head ?: (char *)"", document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

		document->dom = document_parse(document);
	}

	if (!document->dom) {
		return;
	}

	doc = document->dom;

	/* Get root element */
	exc = dom_document_get_document_element(doc, &root);
	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for get_document_element\n");
		//dom_node_unref(doc);
		return;
	} else if (root == NULL) {
		fprintf(stderr, "Broken: root == NULL\n");
		//dom_node_unref(doc);
		return;
	}

	if (!buffer->length) {
		struct string tt;

		if (!init_string(&tt)) {
			return;
		}
		mapa = document->element_map;

		if (!mapa) {
			mapa = create_new_element_map();
			document->element_map = (void *)mapa;
		} else {
			clear_map(mapa);
		}

		if (walk_tree(mapa, &tt, root, true, 0) == false) {
			fprintf(stderr, "Failed to complete DOM structure dump.\n");
			dom_node_unref(root);
			//dom_node_unref(doc);
			return;
		}
		*buffer = tt;
		document->text = tt.source;
	}
	dom_node_unref(root);
	render_plain_document(cached, document, buffer);
}
