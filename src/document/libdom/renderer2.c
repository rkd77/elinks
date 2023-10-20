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
#include "document/html/parser/parse.h"
#include "document/html/renderer.h"
#include "document/libdom/corestrings.h"
#include "document/libdom/doc.h"
#include "document/libdom/mapa.h"
#include "document/libdom/renderer2.h"
#include "ecmascript/libdom/parse.h"

static int in_script = 0;

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

	if (strcmp(dom_string_data(node_name), "BR")) {
		add_to_string(buf, "</");
		add_bytes_to_string(buf, dom_string_data(node_name), dom_string_byte_length(node_name));
		add_char_to_string(buf, '>');
	}

	if (in_script) {
		if (strcmp(dom_string_data(node_name), "SCRIPT") == 0) {
			in_script = 0;
		}
	}

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
dump_dom_element(void *mapa, void *mapa_rev, struct string *buf, dom_node *node, int depth)
{
	dom_exception exc;
	dom_string *node_name = NULL;
	dom_node_type type;
	dom_namednodemap *attrs = NULL;

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

				if (!in_script && length == 1 && *string_text == '<') {
					add_to_string(buf, "&lt;");
				} else if (!in_script && length == 1 && *string_text == '>') {
					add_to_string(buf, "&gt;");
				} else {
					add_bytes_to_string(buf, string_text, length);
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
		fprintf(stderr, "Exception raised for get_node_name\n");
		return false;
	}

	add_char_to_string(buf, '<');
	save_in_map(mapa, node, buf->length);
	save_offset_in_map(mapa_rev, node, buf->length);

	/* Get string data and print element name */
	add_bytes_to_string(buf, dom_string_data(node_name), dom_string_byte_length(node_name));

	if (strcmp(dom_string_data(node_name), "BR") == 0) {
		add_char_to_string(buf, '/');
	} else if (strcmp(dom_string_data(node_name), "SCRIPT") == 0) {
		in_script = 1;
	}

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
	}
	if (attrs) {
		dom_namednodemap_unref(attrs);
	}
	add_char_to_string(buf, '>');

	/* Finished with the node_name dom_string */
	dom_string_unref(node_name);

	return true;
}

static bool
walk_tree(void *mapa, void *mapa_rev, struct string *buf, dom_node *node, bool start, int depth)
{
	dom_exception exc;
	dom_node *child;

	/* Print this node's entry */
	if (dump_dom_element(mapa, mapa_rev, buf, node, depth) == false) {
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
			if (walk_tree(mapa, mapa_rev, buf, child, false, depth) == false) {
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
render_xhtml_document(struct cache_entry *cached, struct document *document, struct string *buffer)
{
	static int initialised = 0;

	if (!initialised) {
		corestrings_init();
		initialised = 1;
	}

	if (!document->dom && !cached->head && buffer && buffer->source) {
		struct string head;

		if (init_string(&head)) {
			scan_http_equiv(buffer->source, buffer->source + buffer->length, &head, NULL, document->cp);
			mem_free_set(&cached->head, head.source);
		}
	}

	if (!document->dom) {
		(void)get_convert_table(cached->head ?: (char *)"", document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);
		document->dom = document_parse(document, buffer);
	}
	dump_xhtml(cached, document, 0);
}

void
dump_xhtml(struct cache_entry *cached, struct document *document, int parse)
{
	dom_exception exc; /* returned by libdom functions */
	dom_document *doc = NULL; /* document, loaded into libdom */
	dom_node *root = NULL; /* root element of document */
	void *mapa = NULL;
	void *mapa_rev = NULL;

	if (!document->dom) {
		return;
	}

	doc = document->dom;

	in_script = 0;
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

	if (1) {
		if (document->text.length) {
			done_string(&document->text);
			if (!init_string(&document->text)) {
				return;
			}
		}
		mapa = document->element_map;

		if (!mapa) {
			mapa = create_new_element_map();
			document->element_map = (void *)mapa;
		} else {
			clear_map(mapa);
		}
		mapa_rev = document->element_map_rev;

		if (!mapa_rev) {
			mapa_rev = create_new_element_map_rev();
			document->element_map_rev = (void *)mapa_rev;
		} else {
			clear_map(mapa_rev);
		}

		if (walk_tree(mapa, mapa_rev, &document->text, root, true, 0) == false) {
			fprintf(stderr, "Failed to complete DOM structure dump.\n");
			dom_node_unref(root);
			//dom_node_unref(doc);
			return;
		}
		dom_node_unref(root);

		if (parse) {
			struct cache_entry *cached2;

			cached->valid = 0;
			cached2 = get_cache_entry(cached->uri);

			if (!cached2) {
				return;
			}
			cached2->head = cached->head;
			cached->head = NULL;

			add_fragment(cached2, 0, document->text.source, document->text.length);
			normalize_cache_entry(cached2, document->text.length);

			object_lock(cached2);
			document->cache_id = cached2->cache_id;
			document->cached = cached2;
			render_xhtml_document(cached2, document, &document->text);
			object_unlock(cached);
			return;
		}
		render_html_document(cached, document, &document->text);
	}
}
