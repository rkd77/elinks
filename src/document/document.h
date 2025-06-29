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

#ifdef __cplusplus
extern "C" {
#endif

struct cache_entry;
struct document_refresh;
struct ecmascript_string_list_item;
struct ecmascript_timeout;
struct el_form_control;
struct frame_desc;
struct frameset_desc;
struct hash;
struct iframe2;
struct image;
struct module;
struct screen_char;
struct string;

/** Nodes are used for marking areas of text on the document canvas as
 * searchable. */
struct node {
	LIST_HEAD_EL(struct node);

	struct el_box box;
};

struct sixel {
	char *pixels;
	unsigned int width;
	unsigned int height;
};

/** The document line consisting of the chars ready to be copied to
 * the terminal screen. */
struct line {
	union {
		struct screen_char *chars;
		//struct sixel *sixel;
	} ch;
	unsigned int length;
};

/** Codepage status */
enum cp_status {
	CP_STATUS_NONE,
	CP_STATUS_SERVER,
	CP_STATUS_ASSUMED,
	CP_STATUS_IGNORED
};

/** Clipboard state */
enum clipboard_status {
	CLIPBOARD_NONE,
	CLIPBOARD_FIRST_POINT,
	CLIPBOARD_SECOND_POINT
};

struct point {
	int x, y;
};


/* Tags are used for ``id''s or anchors in the document referenced by the
 * fragment part of the URI. */
struct tag {
	LIST_HEAD_EL(struct tag);

	int x, y;
	char name[1]; /* must be last of struct. --Zas */
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

enum script_event_hook_type {
	SEVHOOK_ONCLICK,
	SEVHOOK_ONDBLCLICK,
	SEVHOOK_ONMOUSEOVER,
	SEVHOOK_ONHOVER,
	SEVHOOK_ONFOCUS,
	SEVHOOK_ONMOUSEOUT,
	SEVHOOK_ONBLUR,
	SEVHOOK_ONKEYDOWN,
	SEVHOOK_ONKEYUP,
	SEVHOOK_ONKEYPRESS,
	SEVHOOK_ONKEYPRESS_BODY,
};

/* keep in sync with above */
extern const char *script_event_hook_name[];

struct script_event_hook {
	LIST_HEAD_EL(struct script_event_hook);

	enum script_event_hook_type type;
	char *src;
};

struct link {
	unicode_val_T accesskey;

	enum link_type type;

#ifdef CONFIG_ECMASCRIPT
	int element_offset;
#endif
	char *where;
	char *target;
	char *where_img;

	/** The title of the link.  This is in the document charset,
	 * and entities have already been decoded.  */
	char *title;

	/** The set of characters belonging to this link (their coordinates
	 * in the document) - each character has own struct point. */
	struct point *points;
	int npoints;

	int number;
	unsigned int tabindex;

	/** This is supposed to be the colour-pair of the link, but the actual
	 * colours on the canvas can differ--e.g., with image links. */
	struct color_pair color;

	/*! XXX: They don't neccessary need to be link-specific, but we just
	 * don't support them for any other elements for now. Well, we don't
	 * even have a good place where to store them in that case. */
	LIST_OF(struct script_event_hook) *event_hooks;

	union {
		char *name;
		struct el_form_control *form_control;
	} data;
};

struct reverse_link_lookup {
	int number;
	int i;
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
/** A searchable character on the document canvas.
 *
 * struct document.search is an array of struct search, initialised by
 * get_src, q.v.  The elements of the array roughly correspond to the document
 * canvas, document.data; each element corresponds to a regular character, a
 * run of one or more control characters, or the end of a line on the canvas.
 *
 * If an instance of struct search corresponds to a regular character, then that
 * character is stored in @a c and @a n is equal to 1.  If an instance of struct
 * search corresponds to a run of control characters, then a space character ' '
 * is stored in @a c and @a n is equal to to the length of the run.  If an instance
 * of struct search corresponds to the end of a line, then a space is stored in
 * @a c and @a n is equal to 0.
 */
struct search {
	int x, y;               /* Co-ordinates on the document canvas,
	                         * document.data. */

	signed int n;           /* The number of characters on the
	                         * document canvas to which this object
	                         * corresponds.  If the value is 0, then this
	                         * object marks the end of a line.  If the value
	                         * is 1, then this object corresponds to a
	                         * single character.  If the value is greater
	                         * than 1, then this object corresponds to a run
	                         * of control characters. */

	unicode_val_T c;        /* The character on the document canvas to which
	                         * this object corresponds or ' ' if that
	                         * character is a control character or if this
	                         * object marks the end of a line. */

	/* RAM is cheap nowadays */
};
#else
struct search {
	int x, y;
	signed int n:24;	/* This structure is size-critical */
	unsigned char c;
};
#endif

#ifdef CONFIG_ECMASCRIPT
struct offset_linknum {
	int offset, linknum;
};
#endif

struct document {
	OBJECT_HEAD(struct document);

	struct document_options options;

	LIST_OF(struct form) forms;
	LIST_OF(struct tag) tags;
	LIST_OF(struct node) nodes;
	LIST_OF(struct iframe2) iframes;

#ifdef CONFIG_ECMASCRIPT
	/** ECMAScript snippets to be executed during loading the document into
	 * a window.  This currently involves @<script>s and onLoad handlers.
	 * Note that if you hit a string beginning by '^' here, it is followed
	 * by an external reference - you must wait with processing other items
	 * until it gets resolved and loaded. New items are guaranteed to
	 * always appear at the list end. */
	LIST_OF(struct ecmascript_string_list_item) onload_snippets;
	/** @todo FIXME: We should externally maybe using cache_entry store the
	 * dependencies between the various entries so nothing gets removed
	 * unneeded. */
	struct uri_list ecmascript_imports;
	int ecmascript_counter;
	char *body_onkeypress;
	struct offset_linknum *offset_linknum;
#endif
#ifdef CONFIG_LIBDOM
	void *dom;
	void *element_map;
	void *element_map_rev;
	struct string text;
	void *forms_nodeset;
	struct hash *hh;
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
	struct iframeset_desc *iframeset_desc;

	struct uri *uri;

	/* for obtaining IP */
	void *querydns;
	char *ip;
	/** The title of the document.  The charset of this string is
	 * document.options.cp.  */
	char *title;
	struct cache_entry *cached;

	struct frame_desc *frame;
	struct frameset_desc *frame_desc; /**< @todo RENAME ME */
	struct document_refresh *refresh;

	struct line *data;

	struct link *links;

	struct reverse_link_lookup *reverse_link_lookup;
	/** @name Arrays with one item per rendered document's line.
	 * @{ */
	struct link **lines1; /**< The first link on the line. */
	struct link **lines2; /**< The last link on the line. */
	/** @} */

	struct search *search;
	struct search **slines1;
	struct search **slines2;
	struct point *search_points;

#ifdef CONFIG_UTF8
	char buf[7];
	unsigned char buf_length;
#endif
	unsigned int cache_id; /**< Used to check cache entries. */

	int cp;
	int width, height; /**< size of document */
	int nlinks;
	int nsearch;
	int number_of_search_points;

	struct {
		color_T background;
	} color;

	enum cp_status cp_status;
	unsigned int links_sorted:1; /**< whether links are already sorted */
	unsigned int offset_sorted:1;
	unsigned int scanned:1;

	struct el_box clipboard_box;
	enum clipboard_status clipboard_status;
#ifdef CONFIG_KITTY
	LIST_OF(struct k_image) k_images;
#endif
#ifdef CONFIG_LIBSIXEL
	LIST_OF(struct image) images;
#endif
#if defined(CONFIG_KITTY) || defined(CONFIG_LIBSIXEL)
	struct uri_list image_uris;
#endif
};

#define document_has_frames(document_) ((document_) && (document_)->frame_desc)
#define document_has_iframes(document_) ((document_) && (document_)->iframeset_desc)

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

void update_cached_document_options(struct session *ses);

struct document *get_cached_document(struct cache_entry *cached, struct document_options *options);

/** Release a reference to the document.
 * @relates document */
void release_document(struct document *document);

void reset_document(struct document *document);


int get_format_cache_size(void);
int get_format_cache_used_count(void);
int get_format_cache_refresh_count(void);

void shrink_format_cache(int);

#ifdef CONFIG_ECMASCRIPT
int get_link_number_by_offset(struct document *document, int offset);
#endif

extern struct module document_module;

/** @todo FIXME: support for entities and all Unicode characters.
 * (Unpaired surrogates should be rejected, so that the ECMAScript
 * interface can convert the access key to UTF-16.)
 * For now, we only support simple printable character.  */
#define accesskey_string_to_unicode(s) (((s)[0] && !(s)[1] && isprint((s)[0])) ? (s)[0] : 0)

int find_tag(struct document *document, char *name, int namelen);

void insert_document_into_document(struct document *dest, struct document *src, int y);
void remove_document_from_document(struct document *dest, struct document *src, int y);


#ifdef __cplusplus
}
#endif

#endif
