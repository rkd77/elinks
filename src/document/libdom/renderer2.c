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
#include "document/view.h"
#include "ecmascript/libdom/parse.h"
#include "util/hash.h"
#include "util/string.h"

static int libdom_initialised = 0;
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
		add_lowercase_to_string(buf, dom_string_data(node_name), dom_string_byte_length(node_name));
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
	if (mapa) {
		save_in_map(mapa, node, buf->length);
	}
	if (mapa_rev) {
		save_offset_in_map(mapa_rev, node, buf->length);
	}

	/* Get string data and print element name */
	add_lowercase_to_string(buf, dom_string_data(node_name), dom_string_byte_length(node_name));

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
	if (!libdom_initialised) {
		corestrings_init();
		keybstrings_init();
		libdom_initialised = 1;
	}

	if (!document->dom) {
		(void)get_convert_table(cached->head ?: (char *)"", document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

		if (document->cp_status == CP_STATUS_ASSUMED) {
			if (buffer && buffer->source) {
				struct string head;

				if (init_string(&head)) {
					scan_http_equiv(buffer->source, buffer->source + buffer->length, &head, NULL, document->cp);
					(void)get_convert_table(head.source, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);
					done_string(&head);
				}
			}
		}
		document->dom = document_parse(document, buffer);
	}
	dump_xhtml(cached, document, 0);
}

void
free_libdom(void)
{
	if (libdom_initialised) {
		keybstrings_fini();
		corestrings_fini();
	}
}

#if 0
static void
walk_tree2(struct document *document, dom_node *node)
{
	dom_node *child = NULL;
	/* Get the node's first child */
	dom_exception exc = dom_node_get_first_child(node, &child);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for node_get_first_child\n");
		return;
	}

	while (child != NULL) {
		/* Loop though all node's children */
		dom_node *next_child;

		/* Visit node's descendents */
		walk_tree2(document, child);

		/* Go to next sibling */
		exc = dom_node_get_next_sibling(child, &next_child);

		if (exc != DOM_NO_ERR) {
			fprintf(stderr, "Exception raised for node_get_next_sibling\n");
			dom_node_unref(child);
			return;
		}
		dom_node_unref(child);
		child = next_child;
	}

	int offset = find_offset(document->element_map_rev, node);

	if (offset <= 0) {
		return;
	}
	struct node_rect *tab = get_element_rect(document, offset);

	if (!tab) {
		return;
	}
	exc = dom_node_get_first_child(node, &child);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for node_get_first_child\n");
		return;
	}

	while (child != NULL) {
		/* Loop though all node's children */
		dom_node *next_child;
		int offset_i = find_offset(document->element_map_rev, child);

		if (offset_i <= 0) {
			goto next;
		}
		struct node_rect *rect_i = get_element_rect(document, offset_i);

		if (!rect_i) {
			goto next;
		}

		if (rect_i->x0 == INT_MAX) {
			goto next;
		}

		if (tab->x0 > rect_i->x0) {
			tab->x0 = rect_i->x0;
		}
		if (tab->y0 > rect_i->y0) {
			tab->y0 = rect_i->y0;
		}
		if (tab->x1 < rect_i->x1) {
			tab->x1 = rect_i->x1;
		}
		if (tab->y1 < rect_i->y1) {
			tab->y1 = rect_i->y1;
		}
next:
		/* Go to next sibling */
		exc = dom_node_get_next_sibling(child, &next_child);

		if (exc != DOM_NO_ERR) {
			fprintf(stderr, "Exception raised for node_get_next_sibling\n");
			dom_node_unref(child);
			return;
		}
		dom_node_unref(child);
		child = next_child;
	}
}
#endif

#if 0
static void
walk_tree2_color(struct terminal *term, struct el_box *box, struct document *document, int vx, int vy, dom_node *node)
{
	dom_node *child = NULL;
	/* Get the node's first child */
	dom_exception exc = dom_node_get_first_child(node, &child);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for node_get_first_child\n");
		return;
	}
	int offset = find_offset(document->element_map_rev, node);

	if (offset <= 0) {
		return;
	}
	struct node_rect *tab = get_element_rect(document, offset);

	if (!tab) {
		return;
	}

	if (tab->y0 >= document->height) {
		return;
	}

	if (tab->x0 >= document->data[tab->y0].length) {
		return;
	}
	struct screen_char *sc = &document->data[tab->y0].ch.chars[tab->x0];
	int ymax = int_min(document->height, tab->y1 + 1);
	int y, x;

	for (y = int_max(vy, tab->y0);
	     y < int_min(ymax, box->height + vy);
	     y++) {
		int st = int_max(vx, tab->x0);
		int xmax = int_min(box->width + vx, tab->x1 + 1);

		for (x = st; x < xmax; x++) {
			if (x >= document->data[y].length || (document->data[y].ch.chars[x].data == ' ' && document->data[y].ch.chars[x].element_offset == 0)) {
				draw_space(term, box->x + x - vx, box->y + y - vy, sc);
			}
		}
	}
	exc = dom_node_get_first_child(node, &child);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for node_get_first_child\n");
		return;
	}

	while (child != NULL) {
		/* Loop though all node's children */
		dom_node *next_child;

		/* Visit node's descendents */
		walk_tree2_color(term, box, document, vx, vy, child);

		/* Go to next sibling */
		exc = dom_node_get_next_sibling(child, &next_child);

		if (exc != DOM_NO_ERR) {
			fprintf(stderr, "Exception raised for node_get_next_sibling\n");
			dom_node_unref(child);
			return;
		}
		dom_node_unref(child);
		child = next_child;
	}
}
#endif

void
debug_dump_xhtml(void *d)
{
#ifdef ECMASCRIPT_DEBUG
	dom_html_document *doc = (dom_html_document *)d;

	if (!doc) {
		return;
	}
	dom_node *root = NULL;
	dom_exception exc = dom_document_get_document_element(doc, &root);

	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for get_document_element\n");
		//dom_node_unref(doc);
		return;
	} else if (root == NULL) {
		fprintf(stderr, "Broken: root == NULL\n");
		//dom_node_unref(doc);
		return;
	}
	struct string text;

	if (!init_string(&text)) {
		dom_node_unref(root);
		return;
	}

	if (walk_tree(NULL, NULL, &text, root, true, 0) == false) {
		fprintf(stderr, "Failed to complete DOM structure dump.\n");
		dom_node_unref(root);
		done_string(&text);
		//dom_node_unref(doc);
		return;
	}

	fprintf(stderr, "\n---%s\n", text.source);
	done_string(&text);
	dom_node_unref(root);
#endif
}

void
dump_xhtml(struct cache_entry *cached, struct document *document, int parse)
{
	dom_exception exc; /* returned by libdom functions */
	dom_html_document *doc = NULL; /* document, loaded into libdom */
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

		if (mapa) {
			delete_map(mapa);
		}
		mapa = create_new_element_map();
		document->element_map = (void *)mapa;
		mapa_rev = document->element_map_rev;

		if (mapa_rev) {
			delete_map_rev(&mapa_rev);
		}
		mapa_rev = create_new_element_map_rev();
		document->element_map_rev = (void *)mapa_rev;

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

static bool
fire_dom_event(dom_event *event, dom_node *target)
{
	dom_exception exc;
	bool result;

	exc = dom_event_target_dispatch_event(target, event, &result);
	if (exc != DOM_NO_ERR) {
		return false;
	}

	return result;
}

/* Copy from netsurf */
int
fire_generic_dom_event(void *t, void *tar, int bubbles, int cancelable)
{
	dom_string *typ = (dom_string *)t;
	dom_node *target = (dom_node *)tar;
	dom_exception exc;
	dom_event *evt;
	bool result;

	exc = dom_event_create(&evt);
	if (exc != DOM_NO_ERR) return false;
	exc = dom_event_init(evt, typ, bubbles, cancelable);
	if (exc != DOM_NO_ERR) {
		dom_event_unref(evt);
		return false;
	}
//	NSLOG(netsurf, INFO, "Dispatching '%*s' against %p",
//	      (int)dom_string_length(type), dom_string_data(type), target);
	result = fire_dom_event(evt, target);
	dom_event_unref(evt);
	return result;
}

int
fire_onload(void *doc)
{
	return fire_generic_dom_event(corestring_dom_DOMContentLoaded, doc, false, false);
}

#if 0
void
walk2(struct document *document)
{
	dom_exception exc; /* returned by libdom functions */
	dom_html_document *doc = NULL; /* document, loaded into libdom */
	dom_node *root = NULL; /* root element of document */

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
	walk_tree2(document, root);
	dom_node_unref(root);
}
#endif

#if 0
static int prev_offset = 0;
static struct node_rect *prev_element = NULL;

struct node_rect *
get_element_rect(struct document *document, int offset)
{
	if (offset == prev_offset) {
		return prev_element;
	}
	struct hash_item *item = get_hash_item(document->hh, (const char *)&offset, sizeof(offset));

	if (item) {
		prev_offset = offset;
		prev_element = item->value;

		return prev_element;
	}
	struct node_rect *n = mem_alloc(sizeof(*n));

	if (!n) {
		return NULL;
	}
	n->x0 = INT_MAX;
	n->y0 = INT_MAX;
	n->x1 = 0;
	n->y1 = 0;
	n->offset = offset;

	char *key = memacpy((const char *)&offset, sizeof(offset));

	if (key) {
		item = add_hash_item(document->hh, key, sizeof(offset), n);
	}
	if (!item) {
		mem_free(n);
		return NULL;
	}
	prev_offset = offset;
	prev_element = n;

	return n;
}
#endif
//static void dump_results(struct document *document);

#if 0
void
scan_document(struct document_view *doc_view)
{
	int y, x;

	if (!doc_view || !doc_view->document) {
		return;
	}

	if (doc_view->document->hh) {
		free_hash(&doc_view->document->hh);
	}
	doc_view->document->hh = init_hash8();

	if (!doc_view->document->hh) {
		return;
	}
	prev_offset = 0;
	prev_element = NULL;

	for (y = 0; y < doc_view->document->height; y++) {
		for (x = 0; x < doc_view->document->data[y].length; x++) {
			int offset = doc_view->document->data[y].ch.chars[x].element_offset;

			if (!offset) {
				continue;
			}
			struct node_rect *tab = get_element_rect(doc_view->document, offset);

			if (!tab) {
				continue;
			}

			if (tab->x0 > x) {
				tab->x0 = x;
			}
			if (tab->y0 > y) {
				tab->y0 = y;
			}
			if (tab->x1 < x) {
				tab->x1 = x;
			}
			if (tab->y1 < y) {
				tab->y1 = x;
			}
		}
	}
//	dump_results(doc_view->document);
	prev_offset = 0;
	prev_element = NULL;
	walk2(doc_view->document);
//	dump_results(doc_view->document);
}
#endif

#if 0
static void
dump_results(struct document *document)
{
	struct hash_item *item;
	int i;

	foreach_hash_item(item, *(document->hh), i) {
		struct node_rect *rect = item->value;

		fprintf(stderr, "%d:(%d,%d):(%d,%d)\n", rect->offset, rect->x0, rect->y0, rect->x1, rect->y1);
	}
	fprintf(stderr, "=============================\n");
}
#endif

#if 0
void
try_to_color(struct terminal *term, struct el_box *box, struct document *document, int vx, int vy)
{
	dom_exception exc; /* returned by libdom functions */
	dom_html_document *doc = NULL; /* document, loaded into libdom */
	dom_node *root = NULL; /* root element of document */

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
	walk_tree2_color(term, box, document, vx, vy, root);
	dom_node_unref(root);
}
#endif
