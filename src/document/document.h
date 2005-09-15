/* $Id: document.h,v 1.83 2004/12/20 12:12:39 miciah Exp $ */

#ifndef EL__DOCUMENT_DOCUMENT_H
#define EL__DOCUMENT_DOCUMENT_H

#include "document/options.h"
#include "protocol/uri.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/object.h"
#include "util/box.h"

struct cache_entry;
struct document_refresh;
struct form_control;
struct frame_desc;
struct frameset_desc;
struct module;
struct screen_char;

/* Nodes are used for marking areas of text on the document canvas as
 * searchable. */
struct node {
	LIST_HEAD(struct node);

	struct box box;
};


/* The document line consisting of the chars ready to be copied to the terminal
 * screen. */
struct line {
	struct screen_char *chars;
	int length;
};

/* Codepage status */
enum cp_status {
	CP_STATUS_NONE,
	CP_STATUS_SERVER,
	CP_STATUS_ASSUMED,
	CP_STATUS_IGNORED
};


struct point {
	int x, y;
};


enum link_type {
	LINK_HYPERTEXT,
	LINK_MAP,
	LINK_BUTTON,
	LINK_CHECKBOX,
	LINK_SELECT,
	LINK_FIELD,
	LINK_AREA,
};

struct script_event_hook {
	LIST_HEAD(struct script_event_hook);

	enum script_event_hook_type {
		SEVHOOK_ONCLICK,
		SEVHOOK_ONDBLCLICK,
		SEVHOOK_ONMOUSEOVER,
		SEVHOOK_ONHOVER,
		SEVHOOK_ONFOCUS,
		SEVHOOK_ONMOUSEOUT,
		SEVHOOK_ONBLUR,
	} type;
	unsigned char *src;
};

struct link {
	long accesskey;

	enum link_type type;

	unsigned char *where;
	unsigned char *target;
	unsigned char *where_img;
	unsigned char *title;

	/* The set of characters belonging to this link (their coordinates
	 * in the document) - each character has own {struct point}. */
	struct point *points;
	int npoints;

	int number;

	/* This is supposed to be the colour-pair of the link, but the actual
	 * colours on the canvas can differ--e.g., with image links. */
	struct color_pair color;

	/* XXX: They don't neccessary need to be link-specific, but we just
	 * don't support them for any other elements for now. Well, we don't
	 * even have a good place where to store them in that case. */
	struct list_head *event_hooks; /* -> struct script_event_hook */

	union {
		unsigned char *name;
		struct form_control *form_control;
	} data;
};

#define get_link_index(document, link) (link - document->links)

#define link_is_textinput(link) \
	((link)->type == LINK_FIELD || (link)->type == LINK_AREA)

#define link_is_form(link) \
	((link)->type != LINK_HYPERTEXT && (link)->type != LINK_MAP)

#define get_link_form_control(link) \
	(link_is_form(link) ? (link)->data.form_control : NULL)

#define get_link_name(link) \
	(!link_is_form(link) ? (link)->data.name : NULL)


struct search {
	int x, y;
	signed int n:24;	/* This structure is size-critical */
	unsigned char c;
};


struct document {
	LIST_HEAD(struct document);

	struct document_options options;

	struct list_head forms; /* -> struct form */
	struct list_head tags; /* -> struct tag */
	struct list_head nodes; /* -> struct node */

#ifdef CONFIG_ECMASCRIPT
	/* ECMAScript snippets to be executed during loading the document into
	 * a window.  This currently involves <script>s and onLoad handlers.
	 * Note that if you hit a string beginning by '^' here, it is followed
	 * by an external reference - you must wait with processing other items
	 * until it gets resolved and loaded. New items are guaranteed to
	 * always appear at the list end. */
	struct list_head onload_snippets; /* -> struct string_list_item */
	/* FIXME: We should externally maybe using cache_entry store the
	 * dependencies between the various entries so nothing gets removed
	 * unneeded. */
	struct uri_list ecmascript_imports;
#endif
#ifdef CONFIG_CSS
	/* FIXME: We should externally maybe using cache_entry store the
	 * dependencies between the various entries so nothing gets removed
	 * unneeded. */
	struct uri_list css_imports;
	/* Calculated from the id's of all the cache entries in css_imports.
	 * Used for checking rerendering for available CSS imports. */
	unsigned long css_magic;
#endif

	struct uri *uri;
	unsigned char *title;

	struct frame_desc *frame;
	struct frameset_desc *frame_desc; /* RENAME ME */
	struct document_refresh *refresh;

	struct line *data;

	struct link *links;
	/* Arrays with one item per rendered document's line. */
	struct link **lines1; /* The first link on the line. */
	struct link **lines2; /* The last link on the line. */

	struct search *search;
	struct search **slines1;
	struct search **slines2;

	unsigned int id; /* Used to check cache entries. */

	struct object object; /* No direct access, use provided macros for that. */
	int cp;
	int width, height; /* size of document */
	int nlinks;
	int nsearch;
	color_t bgcolor;

	enum cp_status cp_status;
};

#define document_has_frames(document_) ((document_) && (document_)->frame_desc)

/* Initializes a document and its canvas. */
/* Return NULL on allocation failure. */
struct document *
init_document(struct cache_entry *cached, struct document_options *options);

/* Releases the document and all its resources. */
void done_document(struct document *document);

/* Free's the allocated members of the link. */
void done_link_members(struct link *link);

/* Calculates css magic from available CSS imports. Used for determining
 * validity of formatted documents in the cache. */
unsigned long get_document_css_magic(struct document *document);

struct document *get_cached_document(struct cache_entry *cached, struct document_options *options);

/* Release a reference to the document. */
void release_document(struct document *document);

long formatted_info(int);

void shrink_format_cache(int);

extern struct module document_module;

#endif
