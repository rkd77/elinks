#ifndef EL_DOM_SELECT_H
#define EL_DOM_SELECT_H

#include "dom/node.h"


/* FIXME: Namespaces; *|E */

enum dom_select_element_match {
	/* Gives info about the relation required between two element nodes for
	 * them to match. This is also referred to as combinators. */
	/* The following are mutually exclusive and at least one must be set.
	 * DOM_SELECT_RELATION_DESCENDANT is the default. */

	/* Matches any F descendant of E:		E   F */
	/* Bogus flag; it is an easy way to have a default. */
	DOM_SELECT_RELATION_DESCENDANT = 0,
	/* Matches F being a direct child of E:		E > F */
	DOM_SELECT_RELATION_DIRECT_CHILD = 1,
	/* Matches F immediate preceded by E:		E + F */
	DOM_SELECT_RELATION_DIRECT_ADJACENT = 2,
	/* Matches F preceded by E:			E ~ F */
	DOM_SELECT_RELATION_INDIRECT_ADJACENT = 4,

	DOM_SELECT_RELATION_FLAGS = DOM_SELECT_RELATION_DESCENDANT
				  | DOM_SELECT_RELATION_DIRECT_CHILD
				  | DOM_SELECT_RELATION_DIRECT_ADJACENT
				  | DOM_SELECT_RELATION_INDIRECT_ADJACENT,

	/* None of the following are mutual exclusive. They can co-exist
	 * although combining them might not make a lot of sense. */

	/* Matches any element:				* */
	DOM_SELECT_ELEMENT_UNIVERSAL = 8,
	/* Matches the root node of the document:	:root or // */
	DOM_SELECT_ELEMENT_ROOT = 16,
	/* Matches the empty element (not even text):	:empty */
	DOM_SELECT_ELEMENT_EMPTY = 32,

	/* Matches the some n-th child of its parent:	:nth-child(n), etc. */
	DOM_SELECT_ELEMENT_NTH_CHILD = 64,

	/* Matches the some n-th sibling of its type:	:nth-of-type(n), etc. */
	DOM_SELECT_ELEMENT_NTH_TYPE = 128,
};

/* The special CSS .bar class attribute syntax is represented as
 * E[class="bar"]. The ID flag will match against any attribute with it's
 * boolean id member set. XXX: These flags are ATM mutually exclusive. */
enum dom_select_attribute_match {
	/* Matches any set value:			E[foo] */
	DOM_SELECT_ATTRIBUTE_ANY = 1,
	/* Matches exact value "bar":			E[foo="bar"] */
	DOM_SELECT_ATTRIBUTE_EXACT = 2,
	/* Matches space seprated list "z bar bee":	E[foo~="bar"] */
	DOM_SELECT_ATTRIBUTE_SPACE_LIST = 4,
	/* Matches hyphen separated list "z-bar-bee":	E[foo|="bar"] */
	DOM_SELECT_ATTRIBUTE_HYPHEN_LIST = 8,
	/* Matches value begining; "bar-z-bee":		E[foo^="bar"]*/
	DOM_SELECT_ATTRIBUTE_BEGIN = 16,
	/* Matches value ending; "z-bee-bar":		E[foo$="bar"] */
	DOM_SELECT_ATTRIBUTE_END = 32,
	/* Matches value containing; "m33p/bar\++":	E[foo*="bar"] */
	DOM_SELECT_ATTRIBUTE_CONTAINS = 64,
	/* Matches exact ID attribute value "bar":	#bar */
	DOM_SELECT_ATTRIBUTE_ID = 128,
};

/* Info about text matching is stored in a DOM text node. */
enum dom_select_text_match {
	/* Matches E containing substring "foo":	E:contains("foo") */
	DOM_SELECT_TEXT_CONTAINS = 1,
};

/* Info about what nth child or type to match. The basic syntax is:
 *
 * 	<step>n<index>
 *
 * with a little syntactic sugar.
 *
 * Examples:
 *
 *    0n+1 / 1		is first child (same as :first-child)
 *    2n+0 / 2n / even	is all even children
 *    2n+1 / odd	is all odd children
 *   -0n+2		is the last two children
 *   -0n+1 / -1		is last child (same as :last-child)
 *    1n+0 / n+0 / n	is all elements of type
 *    0n+0		is only element of type (a special internal syntax
 *    			used when storing nth-info)
 *
 * That is, a zero step (0n) means exact indexing, and non-zero step
 * means stepwise indexing.
 */
struct dom_select_nth_match {
	size_t step;
	size_t index;
};

#define set_dom_select_nth_match(nth, nthstep, nthindex) \
	do { (nth)->step = (nthstep); (nth)->index = (nthindex); } while(0)

/* This is supposed to be a simple selector. However, this struct is also used
 * for holding data for attribute matching and element text matching. */
struct dom_select_node {
	/* This holds the DOM node which has data about the node being matched.
	 * It can be either an element, attribute, or a text node. */
	/* XXX: Keep at the top. This is used for translating dom_node
	 * reference to dom_select_node. */
	struct dom_node node;

	/* Only meaningful for element nodes. */
	/* FIXME: Don't waste memory for non-element nodes. */
	struct dom_select_nth_match nth_child;
	struct dom_select_nth_match nth_type;

	/* Flags, specifying how the matching should be done. */
	union {
		enum dom_select_element_match element;
		enum dom_select_attribute_match attribute;
		enum dom_select_text_match text;
	} match;
};


enum dom_select_pseudo {
	DOM_SELECT_PSEUDO_UNKNOWN = 0,

	/* Pseudo-elements: */

	/* Matches first formatted line:		::first-line */
	DOM_SELECT_PSEUDO_FIRST_LINE = 1,
	/* Matches first formatted letter:		::first-letter */
	DOM_SELECT_PSEUDO_FIRST_LETTER = 2,
	/* Matches text selected by user:		::selection */
	DOM_SELECT_PSEUDO_SELECTION = 4,
	/* Matches generated context after an element:	::after */
	DOM_SELECT_PSEUDO_AFTER = 8,
	/* Matches generated content before an element:	::before */
	DOM_SELECT_PSEUDO_BEFORE = 16,

	/* Pseudo-attributes: */

	/* Link pseudo-classes: */
	DOM_SELECT_PSEUDO_LINK = 32,			/* :link */
	DOM_SELECT_PSEUDO_VISITED = 64,			/* :visited */

	/* User action pseudo-classes: */
	DOM_SELECT_PSEUDO_ACTIVE = 128,			/* :active */
	DOM_SELECT_PSEUDO_HOVER = 256,			/* :hover */
	DOM_SELECT_PSEUDO_FOCUS = 512,			/* :focus */

	/* Target pseudo-class: */
	DOM_SELECT_PSEUDO_TARGET = 1024,		/* :target */

	/* UI element states pseudo-classes: */
	DOM_SELECT_PSEUDO_ENABLED = 2048,		/* :enabled */
	DOM_SELECT_PSEUDO_DISABLED = 4096,		/* :disabled */
	DOM_SELECT_PSEUDO_CHECKED = 8192,		/* :checked */
	DOM_SELECT_PSEUDO_INDETERMINATE = 16384,	/* :indeterminate */

	/* XXX: The following pseudo-classes are not kept in the pseudo member
	 * of the dom_select struct so they should not be bitfields. They are
	 * mostly for parsing purposes. */

	DOM_SELECT_PSEUDO_CONTAINS = 10000,

	DOM_SELECT_PSEUDO_NTH_CHILD,
	DOM_SELECT_PSEUDO_NTH_LAST_CHILD,
	DOM_SELECT_PSEUDO_FIRST_CHILD,
	DOM_SELECT_PSEUDO_LAST_CHILD,
	DOM_SELECT_PSEUDO_ONLY_CHILD,

	DOM_SELECT_PSEUDO_NTH_TYPE,
	DOM_SELECT_PSEUDO_NTH_LAST_TYPE,
	DOM_SELECT_PSEUDO_FIRST_TYPE,
	DOM_SELECT_PSEUDO_LAST_TYPE,
	DOM_SELECT_PSEUDO_ONLY_TYPE,

	DOM_SELECT_PSEUDO_ROOT,
	DOM_SELECT_PSEUDO_EMPTY,
};

struct dom_select {
	struct dom_select_node *selector;
	unsigned long specificity;
	enum dom_select_pseudo pseudo;
};

enum dom_select_syntax {
	DOM_SELECT_SYNTAX_CSS,	/* Example: 'p a[id=node] a:hover */
	DOM_SELECT_SYNTAX_PATH,	/* Example: '//rss/channel/item' */
};

struct dom_select *init_dom_select(enum dom_select_syntax syntax,
				   struct dom_string *string);

void done_dom_select(struct dom_select *select);

struct dom_node_list *
select_dom_nodes(struct dom_select *select, struct dom_node *root);

/*
 * +------------------------------------------------------------------------------------+
 * | Pattern               | Meaning                      | Type              | Version |
 * |-----------------------+------------------------------+-------------------+---------|
 * | *                     | any element                  | Universal         | 2       |
 * |                       |                              | selector          |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E                     | an element of type E         | Type selector     | 1       |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E F                   | an F element descendant of   | Descendant        | 1       |
 * |                       | an E element                 | combinator        |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E > F                 | an F element child of an E   | Child combinator  | 2       |
 * |                       | element                      |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E + F                 | an F element immediately     | Direct adjacent   | 2       |
 * |                       | preceded by an E element     | combinator        |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E ~ F                 | an F element preceded by an  | Indirect adjacent | 3       |
 * |                       | E element                    | combinator        |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:root                | an E element, root of the    | Structural        | 3       |
 * |                       | document                     | pseudo-classes    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element that has no     | Structural        |         |
 * | E:empty               | children (including text     | pseudo-classes    | 3       |
 * |                       | nodes)                       |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:first-child         | an E element, first child of | Structural        | 2       |
 * |                       | its parent                   | pseudo-classes    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:last-child          | an E element, last child of  | Structural        | 3       |
 * |                       | its parent                   | pseudo-classes    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:nth-child(n)        | an E element, the n-th child | Structural        | 3       |
 * |                       | of its parent                | pseudo-classes    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element, the n-th child | Structural        |         |
 * | E:nth-last-child(n)   | of its parent, counting from | pseudo-classes    | 3       |
 * |                       | the last one                 |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:first-of-type       | an E element, first sibling  | Structural        | 3       |
 * |                       | of its type                  | pseudo-classes    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:last-of-type        | an E element, last sibling   | Structural        | 3       |
 * |                       | of its type                  | pseudo-classes    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:nth-of-type(n)      | an E element, the n-th       | Structural        | 3       |
 * |                       | sibling of its type          | pseudo-classes    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element, the n-th       | Structural        |         |
 * | E:nth-last-of-type(n) | sibling of its type,         | pseudo-classes    | 3       |
 * |                       | counting from the last one   |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:only-child          | an E element, only child of  | Structural        | 3       |
 * |                       | its parent                   | pseudo-classes    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:only-of-type        | an E element, only sibling   | Structural        | 3       |
 * |                       | of its type                  | pseudo-classes    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element being the       |                   |         |
 * | E:link                | source anchor of a hyperlink | The link          |         |
 * | E:visited             | of which the target is not   | pseudo-classes    | 1       |
 * |                       | yet visited (:link) or       |                   |         |
 * |                       | already visited (:visited)   |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:active              | an E element during certain  | The user action   |         |
 * | E:hover               | user actions                 | pseudo-classes    | 1 and 2 |
 * | E:focus               |                              |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:target              | an E element being the       | The target        | 3       |
 * |                       | target of the referring URI  | pseudo-class      |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an element of type E in      |                   |         |
 * | E:lang(fr)            | language "fr" (the document  | The :lang()       | 2       |
 * | FIXME                 | language specifies how       | pseudo-class      |         |
 * |                       | language is determined)      |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:enabled             | a user interface element E   | The UI element    |         |
 * | E:disabled            | which is enabled or disabled | states            | 3       |
 * |                       |                              | pseudo-classes    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | a user interface element E   |                   |         |
 * | E:checked             | which is checked or in an    | The UI element    |         |
 * | E:indeterminate       | indeterminate state (for     | states            | 3       |
 * |                       | instance a radio-button or   | pseudo-classes    |         |
 * |                       | checkbox)                    |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element containing the  | Content           |         |
 * | E:contains("foo")     | substring "foo" in its       | pseudo-class      | 3       |
 * |                       | textual contents             |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E::first-line         | the first formatted line of  | The :first-line   | 1       |
 * |                       | an E element                 | pseudo-element    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E::first-letter       | the first formatted letter   | The :first-letter | 1       |
 * |                       | of an E element              | pseudo-element    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | the portion of an E element  | The UI element    |         |
 * | E::selection          | that is currently            | fragments         | 3       |
 * |                       | selected/highlighted by the  | pseudo-elements   |         |
 * |                       | user                         |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E::before             | generated content before an  | The :before       | 2       |
 * |                       | E element                    | pseudo-element    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E::after              | generated content after an E | The :after        | 2       |
 * |                       | element                      | pseudo-element    |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element whose class is  |                   |         |
 * | E.warning             | "warning" (the document      | Class selectors   | 1       |
 * |                       | language specifies how class |                   |         |
 * |                       | is determined).              |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E#myid                | an E element with ID equal   | ID selectors      | 1       |
 * |                       | to "myid".                   |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E[foo]                | an E element with a "foo"    | Attribute         | 2       |
 * |                       | attribute                    | selectors         |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element whose "foo"     | Attribute         |         |
 * | E[foo="bar"]          | attribute value is exactly   | selectors         | 2       |
 * |                       | equal to "bar"               |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element whose "foo"     |                   |         |
 * |                       | attribute value is a list of | Attribute         |         |
 * | E[foo~="bar"]         | space-separated values, one  | selectors         | 2       |
 * |                       | of which is exactly equal to |                   |         |
 * |                       | "bar"                        |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element whose "foo"     |                   |         |
 * | E[foo^="bar"]         | attribute value begins       | Attribute         | 3       |
 * |                       | exactly with the string      | selectors         |         |
 * |                       | "bar"                        |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element whose "foo"     | Attribute         |         |
 * | E[foo$="bar"]         | attribute value ends exactly | selectors         | 3       |
 * |                       | with the string "bar"        |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element whose "foo"     | Attribute         |         |
 * | E[foo*="bar"]         | attribute value contains the | selectors         | 3       |
 * |                       | substring "bar"              |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * |                       | an E element whose           |                   |         |
 * |                       | "hreflang" attribute has a   | Attribute         |         |
 * | E[hreflang|="en"]     | hyphen-separated list of     | selectors         | 2       |
 * |                       | values beginning (from the   |                   |         |
 * |                       | left) with "en"              |                   |         |
 * |-----------------------+------------------------------+-------------------+---------|
 * | E:not(s)              | an E element that does not   | Negation          | 3       |
 * | FIXME                 | match simple selector s      | pseudo-class      |         |
 * +------------------------------------------------------------------------------------+
 */

#endif
