/** CSS main parser
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/parser.h"
#include "document/css/property.h"
#include "document/css/scanner.h"
#include "document/css/stylesheet.h"
#include "document/css/value.h"
#include "document/html/parser.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


void
css_parse_properties(LIST_OF(struct css_property) *props,
		     struct scanner *scanner)
{
	assert(props && scanner);

	while (scanner_has_tokens(scanner)) {
		struct css_property_info *property_info = NULL;
		struct css_property *prop;
		struct scanner_token *token = get_scanner_token(scanner);
		int i;

		if (!token || token->type == '}') break;

		/* Extract property name. */

		if (token->type != CSS_TOKEN_IDENT
		    || !check_next_scanner_token(scanner, ':')) {
			/* Some use style="{ properties }" so we have to be
			 * check what to skip to. */
			if (token->type == '{') {
				skip_scanner_token(scanner);
			} else {
				skip_css_tokens(scanner, ';');
			}
			continue;
		}

		for (i = 0; css_property_info[i].name; i++) {
			struct css_property_info *info = &css_property_info[i];

			if (scanner_token_strlcasecmp(token, info->name, -1)) {
				property_info = info;
				break;
			}
		}

		/* Skip property name and separator and check for expression */
		if (!skip_css_tokens(scanner, ':')) {
			assert(!scanner_has_tokens(scanner));
			break;
		}

		if (!property_info) {
 			/* Unknown property, check the next one. */
 			goto ride_on;
 		}

		/* We might be on track of something, cook up the struct. */

		prop = mem_calloc(1, sizeof(*prop));
		if (!prop) {
			goto ride_on;
		}
		prop->type = property_info->type;
		prop->value_type = property_info->value_type;
		if (!css_parse_value(property_info, &prop->value, scanner)) {
			mem_free(prop);
			goto ride_on;
		}
		add_to_list(*props, prop);

		/* Maybe we have something else to go yet? */

ride_on:
		skip_css_tokens(scanner, ';');
	}
}

static void
skip_css_block(struct scanner *scanner)
{
	if (skip_css_tokens(scanner, '{')) {
		const int preclimit = get_css_precedence('}');
		int depth = 1;
		struct scanner_token *token = get_scanner_token(scanner);

		while (token && token->precedence <= preclimit && depth > 0) {
			if (token->type == '{')
				++depth;
			else if (token->type == '}')
				--depth;
			token = get_next_scanner_token(scanner);
		}
	}
}

/** Parse an atrule from @a scanner and update @a css accordingly.
 *
 * Atrules grammar:
 *
 * @verbatim
 * media_types:
 *	  <empty>
 *	| <ident>
 *	| media_types ',' <ident>
 *
 * atrule:
 * 	  '@charset' <string> ';'
 *	| '@import' <string> media_types ';'
 *	| '@import' <uri> media_types ';'
 *	| '@media' media_types '{' ruleset* '}'
 *	| '@page' <ident>? [':' <ident>]? '{' properties '}'
 *	| '@font-face' '{' properties '}'
 * @endverbatim
 */
static void
css_parse_atrule(struct css_stylesheet *css, struct scanner *scanner,
		 struct uri *base_uri)
{
	struct scanner_token *token = get_scanner_token(scanner);

	/* Skip skip skip that code */
	switch (token->type) {
		case CSS_TOKEN_AT_IMPORT:
			token = get_next_scanner_token(scanner);
			if (!token) break;

			if (token->type == CSS_TOKEN_STRING
			    || token->type == CSS_TOKEN_URL) {
				assert(css->import);
				css->import(css, base_uri, token->string, token->length);
			}
			skip_css_tokens(scanner, ';');
			break;

		case CSS_TOKEN_AT_CHARSET:
			skip_css_tokens(scanner, ';');
			break;

		case CSS_TOKEN_AT_FONT_FACE:
		case CSS_TOKEN_AT_MEDIA:
		case CSS_TOKEN_AT_PAGE:
			skip_css_block(scanner);
			break;

		case CSS_TOKEN_AT_KEYWORD:
			/* TODO: Unkown @-rule so either skip til ';' or next block. */
			while (scanner_has_tokens(scanner)) {
				token = get_next_scanner_token(scanner);

				if (!token) break;

				if (token->type == ';') {
					skip_scanner_token(scanner);
					break;

				} else if (token->type == '{') {
					skip_css_block(scanner);
					break;
				}
			}
			break;
		default:
			INTERNAL("@-rule parser called without atrule.");
	}
}


struct selector_pkg {
	LIST_HEAD(struct selector_pkg);
	struct css_selector *selector;
};

/** Move a CSS selector and its leaves into a new list.  If a similar
 * selector already exists in the list, merge them.
 *
 * @param sels
 *   The list to which @a selector should be moved.  Must not be NULL.
 * @param selector
 *   The selector that should be moved.  Must not be NULL.  If it is
 *   already in some list, this function removes it from there.
 * @param watch
 *   This function updates @a *watch if it merges that selector into
 *   another one.  @a watch must not be NULL but @a *watch may be.
 *
 * @returns @a selector or the one into which it was merged.  */
static struct css_selector *
reparent_selector(LIST_OF(struct css_selector) *sels,
                  struct css_selector *selector,
                  struct css_selector **watch)
{
	struct css_selector *twin = find_css_selector(sels, selector->type,
	                                              selector->relation,
	                                              selector->name, -1);

	if (twin) {
		merge_css_selectors(twin, selector);
		/* Reparent leaves. */
		while (selector->leaves.next != &selector->leaves) {
			struct css_selector *leaf = selector->leaves.next;

			reparent_selector(&twin->leaves, leaf, watch);
		}
		if (*watch == selector)
			*watch = twin;
		done_css_selector(selector);
	} else {
		if (selector->next) del_from_list(selector);
		add_to_list(*sels, selector);
	}

	return twin ? twin : selector;
}

/** Parse a comma-separated list of CSS selectors from @a scanner.
 * Register the selectors in @a css so that get_css_selector_for_element()
 * will find them, and add them to @a selectors so that the caller can
 * attach properties to them.
 *
 * Our selector grammar:
 *
 * @verbatim
 * selector:
 *	  element_name? ('#' id)? ('.' class)? (':' pseudo_class)? \
 *		  ((' ' | '>') selector)?
 * @endverbatim
 */
static void
css_parse_selector(struct css_stylesheet *css, struct scanner *scanner,
		   LIST_OF(struct selector_pkg) *selectors)
{
	/* Shell for the last selector (the whole selector chain, that is). */
	struct selector_pkg *pkg = NULL;
	/* In 'p#x.y i.z', it's NULL for 'p', 'p' for '#x', '.y' and 'i', and
	 * 'i' for '.z'. */
	struct css_selector *prev_element_selector = NULL;
	/* In 'p#x.y:q i', it's NULL for 'p' and '#x', '#x' for '.y', and '.y'
	 * for ':q', and again NULL for 'i'. */
	struct css_selector *prev_specific_selector = NULL;
	/* In 'p#x.y div.z:a' it is NULL for 'p#x.y' and 'div', and 'p' for
	 * '.z' and ':a'. So the difference from @prev_element_selector is that
	 * it is changed after the current selector fragment is finished, not
	 * right after the base selector is loaded. So it is set differently
	 * for the '#x.y' and '.z:a' parts of selector. */
	struct css_selector *last_chained_selector = NULL;
	/* In 'p#x.y div.z:a, i.b {}', it's set for ':a' and '.b'. */
	int last_fragment = 0;
	/* In 'p#x .y', it's set for 'p' and '.y'. Note that it is always set in
	 * the previous iteration so it's valid for the current token only
	 * before "saving" the token. */
	int selector_start = 1;

	/* FIXME: element can be even '*' --pasky */

	while (scanner_has_tokens(scanner)) {
		struct scanner_token *token = get_scanner_token(scanner);
		struct scanner_token last_token;
		struct css_selector *selector;
		enum css_selector_relation reltype = CSR_ROOT;
		enum css_selector_type seltype = CST_ELEMENT;

		assert(token);
		assert(!last_fragment);


		if (token->type == '{'
		    || token->type == '}'
		    || token->type == ';')
			break;


		/* Examine the selector fragment */

		if (token->type != CSS_TOKEN_IDENT) {
			switch (token->type) {
			case CSS_TOKEN_HASH:
			case CSS_TOKEN_HEX_COLOR:
				seltype = CST_ID;
				reltype = selector_start ? CSR_ANCESTOR : CSR_SPECIFITY;
				break;

			case '.':
				seltype = CST_CLASS;
				reltype = selector_start ? CSR_ANCESTOR : CSR_SPECIFITY;
				break;

			case ':':
				seltype = CST_PSEUDO;
				reltype = selector_start ? CSR_ANCESTOR : CSR_SPECIFITY;
				break;

			case '>':
				seltype = CST_ELEMENT;
				reltype = CSR_PARENT;
				break;

			default:
				/* FIXME: Temporary fix for this weird CSS
				 * precedence thing. ')' has higher than ','
				 * and it can cause problems when skipping
				 * here. The reason is for the function()
				 * parsing. Hmm... --jonas */
				if (!skip_css_tokens(scanner, ','))
					skip_scanner_token(scanner);
				seltype = CST_INVALID;
				break;
			}

			if (seltype == CST_INVALID)
				continue;

			/* Hexcolor and hash already contains the ident
			 * inside. */
			if (token->type != CSS_TOKEN_HEX_COLOR
			    && token->type != CSS_TOKEN_HASH) {
				token = get_next_scanner_token(scanner);
				if (!token) break;
				if (token->type != CSS_TOKEN_IDENT) /* wtf */
					continue;
			} else {
				/* Skip the leading '#'. */
				token->string++, token->length--;
			}

		} else {
			if (pkg) reltype = CSR_ANCESTOR;
		}


		/* Look ahead at what's coming next */

		copy_struct(&last_token, token);
		/* Detect whether upcoming tokens are separated by
		 * whitespace or not (that's important for determining
		 * whether it's a combinator or specificitier). */
		if (last_token.string + last_token.length < scanner->end) {
			selector_start = last_token.string[last_token.length];
			selector_start = (selector_start != '#'
			                  && selector_start != '.'
			                  && selector_start != ':');
		} /* else it doesn't matter as we are gonna bail out anyway. */

		token = get_next_scanner_token(scanner);
		if (!token) break;
		last_fragment = (token->type == ',' || token->type == '{');


		/* Register the selector */

		if (!pkg) {
			selector = get_css_base_selector(
			                last_fragment ? css : NULL, seltype,
					CSR_ROOT,
					last_token.string, last_token.length);
			if (!selector) continue;

			pkg = mem_calloc(1, sizeof(*pkg));
			if (!pkg) continue;
			add_to_list(*selectors, pkg);
			pkg->selector = selector;

		} else if (reltype == CSR_SPECIFITY) {
			/* We append under the last fragment. */
			struct css_selector *base_sel = prev_specific_selector;

			if (!base_sel) base_sel = prev_element_selector;
			assert(base_sel);

			selector = get_css_selector(&base_sel->leaves,
			                            seltype, reltype,
						    last_token.string,
						    last_token.length);
			if (!selector) continue;

			if (last_chained_selector) {
				/* The situation is like: 'div p#x', now it was
				 * 'p -> div', but we need to redo that as
				 * '(p ->) #x -> div'. */
				del_from_list(last_chained_selector);
				add_to_list(selector->leaves,
				            last_chained_selector);
			}

			if (pkg->selector == base_sel) {
				/* This is still just specificitying offspring
				 * of the previous pkg->selector. */
				pkg->selector = selector;
			}

			if (last_fragment) {
				/* This is the last fragment of the selector
				 * chain, that means the last base fragment
				 * wasn't marked so and thus wasn't bound to
				 * the stylesheet. Let's do that now. */
				assert(prev_element_selector);
				prev_element_selector->relation = CSR_ROOT;
				prev_element_selector =
					reparent_selector(&css->selectors,
					                 prev_element_selector,
							 &pkg->selector);
			}

		} else /* CSR_PARENT || CSR_ANCESTOR */ {
			/* We - in the perlish speak - unshift in front
			 * of the previous selector fragment and reparent
			 * it to the upcoming one. */
			selector = get_css_base_selector(
			                last_fragment ? css : NULL, seltype,
					CSR_ROOT,
					last_token.string, last_token.length);
			if (!selector) continue;

			assert(prev_element_selector);
			add_to_list(selector->leaves, prev_element_selector);
			last_chained_selector = prev_element_selector;

			prev_element_selector->relation = reltype;
		}


		/* Record the selector fragment for future generations */

		if (reltype == CSR_SPECIFITY) {
			prev_specific_selector = selector;
		} else {
			prev_element_selector = selector;
			prev_specific_selector = NULL;
		}


		/* What to do next */

		if (last_fragment) {
			/* Next selector coming, clean up. */
			pkg = NULL; last_fragment = 0; selector_start = 1;
			prev_element_selector = NULL;
			prev_specific_selector = NULL;
			last_chained_selector = NULL;
		}

		if (token->type == ',') {
			/* Another selector hooked to these properties. */
			skip_scanner_token(scanner);

		} else if (token->type == '{') {
			/* End of selector list. */
			break;

		} /* else Another selector fragment probably coming up. */
	}

	/* Wipe the selector we were currently composing, if any. */
	if (pkg) {
		if (prev_element_selector)
			done_css_selector(prev_element_selector);
		del_from_list(pkg);
		mem_free(pkg);
	}
}


/** Parse a ruleset from @a scanner to @a css.
 *
 * Ruleset grammar:
 *
 * @verbatim
 * ruleset:
 *	  selector [ ',' selector ]* '{' properties '}'
 * @endverbatim
 */
static void
css_parse_ruleset(struct css_stylesheet *css, struct scanner *scanner)
{
	INIT_LIST_OF(struct selector_pkg, selectors);
	INIT_LIST_OF(struct css_property, properties);
	struct selector_pkg *pkg;

	css_parse_selector(css, scanner, &selectors);
	if (list_empty(selectors)
	    || !skip_css_tokens(scanner, '{')) {
		if (!list_empty(selectors)) free_list(selectors);
		skip_css_tokens(scanner, '}');
		return;
	}


	/* We don't handle the case where a property has already been added to
	 * a selector. That doesn't matter though, because the best one will be
	 * always the last one (FIXME: 'important!'), therefore the applier
	 * will take it last and it will have the "final" effect.
	 *
	 * So it's only a little waste and no real harm. The thing is, what do
	 * you do when you have 'background: #fff' and then 'background:
	 * x-repeat'? It would require yet another logic to handle merging of
	 * these etc and the induced overhead would in most cases mean more
	 * waste that having the property multiple times in a selector, I
	 * believe. --pasky */

	pkg = selectors.next;
	css_parse_properties(&properties, scanner);

	skip_css_tokens(scanner, '}');

	/* Mirror the properties to all the selectors. */
	foreach (pkg, selectors) {
#ifdef DEBUG_CSS
		/* Cannot use list_empty() inside the arglist of DBG()
		 * because GCC 4.1 "warning: operation on `errfile'
		 * may be undefined" breaks the build with -Werror.  */
		int dbg_has_properties = !list_empty(properties);
		int dbg_has_leaves = !list_empty(pkg->selector->leaves);

		DBG("Binding properties (!!%d) to selector %s (type %d, relation %d, children %d)",
			dbg_has_properties,
			pkg->selector->name, pkg->selector->type,
			pkg->selector->relation,
			dbg_has_leaves);
#endif
		add_selector_properties(pkg->selector, &properties);
	}
	free_list(selectors);
	free_list(properties);
}


void
css_parse_stylesheet(struct css_stylesheet *css, struct uri *base_uri,
		     unsigned char *string, unsigned char *end)
{
	struct scanner scanner;

	init_scanner(&scanner, &css_scanner_info, string, end);

	while (scanner_has_tokens(&scanner)) {
		struct scanner_token *token = get_scanner_token(&scanner);

		assert(token);

		switch (token->type) {
		case CSS_TOKEN_AT_KEYWORD:
		case CSS_TOKEN_AT_CHARSET:
		case CSS_TOKEN_AT_FONT_FACE:
		case CSS_TOKEN_AT_IMPORT:
		case CSS_TOKEN_AT_MEDIA:
		case CSS_TOKEN_AT_PAGE:
			css_parse_atrule(css, &scanner, base_uri);
			break;

		default:
			/* And WHAT ELSE could it be?! */
			css_parse_ruleset(css, &scanner);
		}
	}
#ifdef DEBUG_CSS
	dump_css_selector_tree(&css->selectors);
	WDBG("That's it.");
#endif
}
