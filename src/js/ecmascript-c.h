#ifndef EL__ECMASCRIPT_ECMASCRIPT_C_H
#define EL__ECMASCRIPT_ECMASCRIPT_C_H

#include "document/document.h"
#include "js/libdom/dom.h"
#include "main/module.h"
#include "terminal/kbd.h"
#include "util/lists.h"

#ifdef __cplusplus
extern "C" {
#endif

struct document;
struct document_options;
struct document_view;
struct ecmascript_interpreter;
struct form_state;
struct form_view;
struct session;
struct string;
struct term_event;
struct uri;
struct view_state;

struct selector_node {
	LIST_HEAD_EL(struct selector_node);

	void *node;
};

struct class_string {
	LIST_HEAD_EL(struct class_string);

	void *name;
};

typedef bool (*dom_callback_is_in_collection)(struct dom_node *node, void *ctx);

/**
 * The html_collection structure
 */
struct el_dom_html_collection {
	dom_callback_is_in_collection ic;
	/**< The function pointer used to test
	 * whether some node is an element of
	 * this collection
	 */
	void *ctx; /**< Context for the callback */
	struct dom_html_document *doc;  /**< The document created this
                                     * collection
                                     */
	dom_node *root; /**< The root node of this collection */
	uint32_t refcnt; /**< Reference counting */
};

int ecmascript_get_interpreter_count(void);
void ecmascript_put_interpreter(struct ecmascript_interpreter *interpreter);
void toggle_ecmascript(struct session *ses);

/* Takes line with the syntax javascript:<ecmascript code>. Activated when user
 * follows a link with this synstax. */
void ecmascript_protocol_handler(struct session *ses, struct uri *uri);
void check_for_snippets(struct view_state *vs, struct document_options *options, struct document *document);

void check_events_for_element(struct ecmascript_interpreter *interpreter, dom_node *element, struct term_event *ev);
void ecmascript_reset_state(struct view_state *vs);
int ecmascript_current_link_evhook(struct document_view *doc_view, enum script_event_hook_type type);
int ecmascript_eval_boolback(struct ecmascript_interpreter *interpreter, struct string *code);

void ecmascript_detach_form_view(struct form_view *fv);
void ecmascript_detach_form_state(struct form_state *fs);
void ecmascript_moved_form_state(struct form_state *fs);

void *walk_tree_query(dom_node *node, const char *selector, int depth);
void walk_tree_query_append(dom_node *node, const char *selector, int depth, LIST_OF(struct selector_node) *result_list);
void *get_elements_by_class_name(dom_html_document *doc, dom_node *node, const char *classes);

void camel_to_html(const char *camel, struct string *result);
void ecmascript_walk_tree(struct string *buf, void *nod, bool start, bool toSortAttrs);

void free_el_dom_collection(void *ctx);

void save_integrity_in_map(struct uri *uri, char *integrity);
char *get_integrity_from_map(struct uri *uri);

extern struct module ecmascript_module;

#ifdef __cplusplus
}
#endif

#endif
