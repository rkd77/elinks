
#ifndef EL__DOCUMENT_CSS_STYLESHEET_H
#define EL__DOCUMENT_CSS_STYLESHEET_H

#include "protocol/uri.h"
#include "util/lists.h"

/* #define DEBUG_CSS */

/** @file
 * @todo TODO: We need a memory efficient and fast way to define how
 * properties cascade. What we are interested in is making it fast and
 * easy to find all properties we need.
 *
 * @code
 *	struct css_cascade {
 *		struct css_cascade *parent;
 *		struct list_head properties;
 *
 *			- Can later be turned into a table to not waste memory:
 *			  struct css_property properties[1];
 *	};
 * @endcode
 *
 * And the selector should then only map a document element into this
 * data structure.
 *
 * All the CSS applier has to do is require the css_cascade of the current
 * element and it should nicely inherit any style from parent cascades.
 * Question is in what direction to apply. It should be possible for the user
 * to overwrite any document provided stylesheet using "!important" so we need
 * to keep track in some table what properties was already applied so we only
 * overwrite when we have to. --jonas
 *
 * XXX: This is one of the TODOs where I have no clue what is it talking about
 * in particular. Is it obsolete now when we grok 'td.foo p#x>a:hover' without
 * hesitation? --pasky */


/** The struct css_selector is used for mapping elements (or nodes) in the
 * document structure to properties. See README for some hints about how the
 * trees of these span.
 *
 * Selectors are currently kept in lists that find_css_selector() then
 * has to search linearly.  Hashing was also tested but did not help
 * in practice: each find_css_selector() call runs approximately one
 * strcasecmp(), and a hash function is unlikely to be faster than that.
 * See ELinks bug 789 for details.  */
struct css_selector {
	LIST_HEAD(struct css_selector);

	/** This defines relation between this selector fragment and its
	 * parent in the selector tree. */
	enum css_selector_relation {
		CSR_ROOT, /**< First class stylesheet member. */
		CSR_SPECIFITY, /**< Narrowing-down, i.e. the "x" in "foo#x". */
		CSR_ANCESTOR, /**< Ancestor, i.e. the "p" in "p a". */
		CSR_PARENT, /**< Direct parent, i.e. the "div" in "div>img". */
	} relation;
	LIST_OF(struct css_selector) leaves;

	enum css_selector_type {
		CST_ELEMENT,
		CST_ID,
		CST_CLASS,
		CST_PSEUDO,
		CST_INVALID, /**< Auxiliary for the parser */
	} type;
	unsigned char *name;

	LIST_OF(struct css_property) properties;
};


struct css_stylesheet;
typedef void (*css_stylesheet_importer_T)(struct css_stylesheet *, struct uri *,
					  const unsigned char *url, int urllen);

/** The struct css_stylesheet describes all the useful data that was extracted
 * from the CSS source. Currently we don't cache anything but the default user
 * stylesheet so it can contain stuff from both @<style> tags and @@import'ed
 * CSS documents. */
struct css_stylesheet {
	/** The import callback function. */
	css_stylesheet_importer_T import;

	/** The import callback's data. */
	void *import_data;

	/** The list of basic element selectors (which can then somehow
	 * tree up on inside). */
	LIST_OF(struct css_selector) selectors;

	/** How deeply nested are we. Limited by MAX_REDIRECTS. */
	int import_level;
};

#define INIT_CSS_STYLESHEET(css, import) \
	{ import, NULL, { D_LIST_HEAD(css.selectors) } }

/** Dynamically allocates a stylesheet. */
struct css_stylesheet *init_css_stylesheet(css_stylesheet_importer_T importer,
                                           void *import_data);

/** Mirror given CSS stylesheet @a css1 to an identical copy of itself
 * (including all the selectors), @a css2. */
void mirror_css_stylesheet(struct css_stylesheet *css1,
			   struct css_stylesheet *css2);

/** Releases all the content of the stylesheet (but not the stylesheet
 * itself). */
void done_css_stylesheet(struct css_stylesheet *css);


/** Returns a new freshly made selector adding it to the given selector
 * list, or NULL. */
struct css_selector *get_css_selector(LIST_OF(struct css_selector) *selector_list,
                                      enum css_selector_type type,
                                      enum css_selector_relation rel,
                                      const unsigned char *name, int namelen);

#define get_css_base_selector(stylesheet, type, rel, name, namelen) \
	get_css_selector((stylesheet) ? &(stylesheet)->selectors : NULL, \
	                 type, rel, name, namelen)

/** Looks up the selector of the name @a name and length @a namelen in
 * the given list of selectors. */
struct css_selector *find_css_selector(LIST_OF(struct css_selector) *selector_list,
                                       enum css_selector_type type,
                                       enum css_selector_relation rel,
                                       const unsigned char *name, int namelen);

#define find_css_base_selector(stylesheet, type, rel, name, namelen) \
	find_css_selector(&stylesheet->selectors, rel, type, name, namelen)

/** Initialize the selector structure. This is a rather low-level
 * function from your POV. */
struct css_selector *init_css_selector(LIST_OF(struct css_selector) *selector_list,
                                       enum css_selector_type type,
                                       const unsigned char *name, int namelen);

/** Add all properties from the list to the given @a selector. */
void add_selector_properties(struct css_selector *selector,
                             LIST_OF(struct css_property) *properties);

/** Join @a sel2 to @a sel1, @a sel1 taking precedence in all conflicts. */
void merge_css_selectors(struct css_selector *sel1, struct css_selector *sel2);

/** Destroy a selector. done_css_stylesheet() normally does that for you. */
void done_css_selector(struct css_selector *selector);

#ifdef DEBUG_CSS
/** Dumps the selector tree to stderr. */
void dump_css_selector_tree(LIST_OF(struct css_selector) *selector_list);
#endif

#endif
