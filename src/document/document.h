#ifndef EL__DOCUMENT_DOCUMENT_H
#define EL__DOCUMENT_DOCUMENT_H

#include "document/options.h"
#include "intl/charsets.h" /* unicode_val_T */
#include "main/object.h"
#include "main/timer.h"
#include "protocol/uri.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/box.h"

struct cache_entry;
struct document_refresh;
struct form_control;
struct frame_desc;
struct frameset_desc;
struct module;
struct screen_char;

/** Nodes are used for marking areas of text on the document canvas as
 * searchable. */
struct node {
	LIST_HEAD(struct node);

	struct box box;
};


/** The document line consisting of the chars ready to be copied to
 * the terminal screen. */
struct line {
	struct screen_char *chars;
	int length;
};

/** Codepage status */
enum cp_status {
	CP_STATUS_NONE,
	CP_STATUS_SERVER,
	CP_STATUS_ASSUMED,
	CP_STATUS_IGNORED
};


struct point {
	int x, y;
};


/* Tags are used for ``id''s or anchors in the document referenced by the
 * fragment part of the URI. */
struct tag {
	LIST_HEAD(struct tag);

	int x, y;
	unsigned char name[1]; /* must be last of struct. --Zas */
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
	unicode_val_T accesskey;

	enum link_type type;

	unsigned char *where;
	unsigned char *target;
	unsigned char *where_img;
	unsigned char *title;

	/** The set of characters belonging to this link (their coordinates
	 * in the document) - each character has own struct point. */
	struct point *points;
	int npoints;

	int number;

	/** This is supposed to be the colour-pair of the link, but the actual
	 * colours on the canvas can differ--e.g., with image links. */
	struct color_pair color;

	/*! XXX: They don't neccessary need to be link-specific, but we just
	 * don't support them for any other elements for now. Well, we don't
	 * even have a good place where to store them in that case. */
	LIST_OF(struct script_event_hook) *event_hooks;

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

#ifdef CONFIG_UTF8
struct search {
	int x, y;
	signed int n;		/* RAM is cheap nowadays */
	unicode_val_T c;
};
#else
struct search {
	int x, y;
	signed int n:24;	/* This structure is size-critical */
	unsigned char c;
};
#endif

struct document {
	OBJECT_HEAD(struct document);

	struct document_options options;

	LIST_OF(struct form) forms;
	LIST_OF(struct tag) tags;
	LIST_OF(struct node) nodes;

#ifdef CONFIG_ECMASCRIPT
	/** ECMAScript snippets to be executed during loading the document into
	 * a window.  This currently involves @<script>s and onLoad handlers.
	 * Note that if you hit a string beginning by '^' here, it is followed
	 * by an external reference - you must wait with processing other items
	 * until it gets resolved and loaded. New items are guaranteed to
	 * always appear at the list end. */
	LIST_OF(struct string_list_item) onload_snippets;
	/** @todo FIXME: We should externally maybe using cache_entry store the
	 * dependencies between the various entries so nothing gets removed
	 * unneeded. */
	struct uri_list ecmascript_imports;
	/** used by setTimeout */
	timer_id_T timeout;
#endif
#ifdef CONFIG_CSS
	/** @todo FIXME: We should externally maybe using cache_entry store the
	 * dependencies between the various entries so nothing gets removed
	 * unneeded. */
	struct uri_list css_imports;
	/** Calculated from the id's of all the cache entries in #css_imports.
	 * Used for checking rerendering for available CSS imports. */
	unsigned long css_magic;
#endif

	struct uri *uri;

	/** The title of the document.  The charset of this string is
	 * document.options.cp.  */
	unsigned char *title;
	struct cache_entry *cached;

	struct frame_desc *frame;
	struct frameset_desc *frame_desc; /**< @todo RENAME ME */
	struct document_refresh *refresh;

	struct line *data;

	struct link *links;
	/** @name Arrays with one item per rendered document's line.
	 * @{ */
	struct link **lines1; /**< The first link on the line. */
	struct link **lines2; /**< The last link on the line. */
	/** @} */

	struct search *search;
	struct search **slines1;
	struct search **slines2;

#ifdef CONFIG_UTF8
	unsigned char buf[7];
	unsigned char buf_length;
#endif
	unsigned int cache_id; /**< Used to check cache entries. */

	int cp;
	int width, height; /**< size of document */
	int nlinks;
	int nsearch;
	color_T bgcolor;

	enum cp_status cp_status;
	unsigned int links_sorted:1; /**< whether links are already sorted */
};

#define document_has_frames(document_) ((document_) && (document_)->frame_desc)

/** Initializes a document and its canvas.
 * @returns NULL on allocation failure.
 * @relates document */
struct document *
init_document(struct cache_entry *cached, struct document_options *options);

/** Releases the document and all its resources.
 * @relates document */
void done_document(struct document *document);

/** Free's the allocated members of the link. */
void done_link_members(struct link *link);

/** Calculates css magic from available CSS imports. Used for determining
 * validity of formatted documents in the cache. */
unsigned long get_document_css_magic(struct document *document);

void update_cached_document_options(void);

struct document *get_cached_document(struct cache_entry *cached, struct document_options *options);

/** Release a reference to the document.
 * @relates document */
void release_document(struct document *document);

int get_format_cache_size(void);
int get_format_cache_used_count(void);
int get_format_cache_refresh_count(void);

void shrink_format_cache(int);

extern struct module document_module;

/** @todo FIXME: support for entities and all Unicode characters.
 * (Unpaired surrogates should be rejected, so that the ECMAScript
 * interface can convert the access key to UTF-16.)
 * For now, we only support simple printable character.  */
#define accesskey_string_to_unicode(s) (((s)[0] && !(s)[1] && isprint((s)[0])) ? (s)[0] : 0)

int find_tag(struct document *document, unsigned char *name, int namelen);

#endif
