
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

/** A set of struct css_selector.  This is currently represented as a
 * list but that may be changed later.  Therefore please try not to
 * access the contents directly; instead define new wrapper macros.
 *
 * According to CSS2 section 7.1 "Cascading order", if two rules have
 * the same weight, then the latter specified wins.  Regardless, the
 * order of rules need not be represented in struct css_selector_set,
 * because all rules for the same selector have already been merged
 * into one struct css_selector.  */
struct css_selector_set {
	unsigned char may_contain_rel_ancestor_or_parent;

	/** The list of selectors in this set.
	 *
	 * Sets are currently represented as lists that
	 * find_css_selector() then has to search linearly.
	 * Hashing was also tested but did not help in practice:
	 * each find_css_selector() call runs approximately one
	 * strcasecmp(), and a hash function is unlikely to be
	 * faster than that.  See ELinks bug 789 for details.
	 *
	 * Keep this away from the beginning of the structure,
	 * so that nobody can cast the struct css_selector_set *
	 * to LIST_OF(struct css_selector) * and get away with it.  */
	LIST_OF(struct css_selector) list;
};
#define INIT_CSS_SELECTOR_SET(set) { 0, { D_LIST_HEAD(set.list) } }

enum css_selector_relation {
	CSR_ROOT, /**< First class stylesheet member. */
	CSR_SPECIFITY, /**< Narrowing-down, i.e. the "x" in "foo#x". */
	CSR_ANCESTOR, /**< Ancestor, i.e. the "p" in "p a". */
	CSR_PARENT, /**< Direct parent, i.e. the "div" in "div>img". */
};

enum css_selector_type {
	CST_ELEMENT,
	CST_ID,
	CST_CLASS,
	CST_PSEUDO,
	CST_INVALID, /**< Auxiliary for the parser */
};

/** The struct css_selector is used for mapping elements (or nodes) in the
 * document structure to properties. See README for some hints about how the
 * trees of these span. */
struct css_selector {
	LIST_HEAD(struct css_selector);

	/** This defines relation between this selector fragment and its
	 * parent in the selector tree.
	 * Update with set_css_selector_relation().  */
	enum css_selector_relation  relation;
	struct css_selector_set leaves;

	enum css_selector_type type;
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
	/** The import callback function.  The caller must check the
	 * media types first.  */
	css_stylesheet_importer_T import;

	/** The import callback's data. */
	void *import_data;

	/** The set of basic element selectors (which can then somehow
	 * tree up on inside). */
	struct css_selector_set selectors;

	/** How deeply nested are we. Limited by MAX_REDIRECTS. */
	int import_level;
};

#define INIT_CSS_STYLESHEET(css, import) \
	{ import, NULL, INIT_CSS_SELECTOR_SET(css.selectors) }

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
 * set, or NULL. */
struct css_selector *get_css_selector(struct css_selector_set *set,
                                      enum css_selector_type type,
                                      enum css_selector_relation rel,
                                      const unsigned char *name, int namelen);

#define get_css_base_selector(stylesheet, type, rel, name, namelen) \
	get_css_selector((stylesheet) ? &(stylesheet)->selectors : NULL, \
	                 type, rel, name, namelen)

/** Looks up the selector of the name @a name and length @a namelen in
 * the given set of selectors. */
struct css_selector *find_css_selector(struct css_selector_set *set,
                                       enum css_selector_type type,
                                       enum css_selector_relation rel,
                                       const unsigned char *name, int namelen);

#define find_css_base_selector(stylesheet, type, rel, name, namelen) \
	find_css_selector(&stylesheet->selectors, rel, type, name, namelen)

/** Initialize the selector structure. This is a rather low-level
 * function from your POV. */
struct css_selector *init_css_selector(struct css_selector_set *set,
                                       enum css_selector_type type,
                                       enum css_selector_relation relation,
                                       const unsigned char *name, int namelen);

/** Add all properties from the list to the given @a selector. */
void add_selector_properties(struct css_selector *selector,
                             LIST_OF(struct css_property) *properties);

/** Join @a sel2 to @a sel1, @a sel1 taking precedence in all conflicts. */
void merge_css_selectors(struct css_selector *sel1, struct css_selector *sel2);

/** Use this function instead of modifying css_selector.relation directly.  */
void set_css_selector_relation(struct css_selector *,
			       enum css_selector_relation);

/** Destroy a selector. done_css_stylesheet() normally does that for you. */
void done_css_selector(struct css_selector *selector);

void init_css_selector_set(struct css_selector_set *set);
void done_css_selector_set(struct css_selector_set *set);
#define css_selector_set_empty(set) list_empty((set)->list)
#define css_selector_set_front(set) ((struct css_selector *) ((set)->list.next))
void add_css_selector_to_set(struct css_selector *, struct css_selector_set *);
void del_css_selector_from_set(struct css_selector *);
#define css_selector_is_in_set(selector) ((selector)->next != NULL)
#define foreach_css_selector(selector, set) foreach (selector, (set)->list)

#ifdef DEBUG_CSS
/** Dumps the selector tree to stderr. */
void dump_css_selector_tree(struct css_selector_set *set);
#endif

#endif
