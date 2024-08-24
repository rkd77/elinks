/* CSS */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include <stdio.h>
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#include <libcss/libcss.h>

#include "cache/cache.h"
#include "document/html/internal.h"
#include "document/libdom/css.h"
#include "document/libdom/mapa.h"
#include "document/libdom/corestrings.h"
#include "util/string.h"

#define UNUSED(a)

static css_error node_name(void *pw, void *node, css_qname *qname);
static css_error node_classes(void *pw, void *node,
		lwc_string ***classes, uint32_t *n_classes);
static css_error node_id(void *pw, void *node, lwc_string **id);
static css_error named_parent_node(void *pw, void *node,
		const css_qname *qname, void **parent);
static css_error named_sibling_node(void *pw, void *node,
		const css_qname *qname, void **sibling);
static css_error named_generic_sibling_node(void *pw, void *node,
		const css_qname *qname, void **sibling);
static css_error parent_node(void *pw, void *node, void **parent);
static css_error sibling_node(void *pw, void *node, void **sibling);
static css_error node_has_name(void *pw, void *node,
		const css_qname *qname, bool *match);
static css_error node_has_class(void *pw, void *node,
		lwc_string *name, bool *match);
static css_error node_has_id(void *pw, void *node,
		lwc_string *name, bool *match);
static css_error node_has_attribute(void *pw, void *node,
		const css_qname *qname, bool *match);
static css_error node_has_attribute_equal(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_has_attribute_dashmatch(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_has_attribute_includes(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_has_attribute_prefix(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_has_attribute_suffix(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_has_attribute_substring(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match);
static css_error node_is_root(void *pw, void *node, bool *match);
static css_error node_count_siblings(void *pw, void *node,
		bool same_name, bool after, int32_t *count);
static css_error node_is_empty(void *pw, void *node, bool *match);
static css_error node_is_link(void *pw, void *node, bool *match);
static css_error node_is_hover(void *pw, void *node, bool *match);
static css_error node_is_active(void *pw, void *node, bool *match);
static css_error node_is_focus(void *pw, void *node, bool *match);
static css_error node_is_enabled(void *pw, void *node, bool *match);
static css_error node_is_disabled(void *pw, void *node, bool *match);
static css_error node_is_checked(void *pw, void *node, bool *match);
static css_error node_is_target(void *pw, void *node, bool *match);
static css_error node_is_lang(void *pw, void *node,
		lwc_string *lang, bool *match);
static css_error ua_default_for_property(void *pw, uint32_t property,
		css_hint *hint);
static css_error set_libcss_node_data(void *pw, void *node,
		void *libcss_node_data);
static css_error get_libcss_node_data(void *pw, void *node,
		void **libcss_node_data);

static css_error named_ancestor_node(void *pw, void *node,
		const css_qname *qname, void **ancestor);

static css_error node_is_visited(void *pw, void *node, bool *match);

static css_error node_presentational_hint(void *pw, void *node,
		uint32_t *nhints, css_hint **hints);

static css_error node_presentational_hint(void *pw, void *node,
		uint32_t *nhints, css_hint **hints)
{
//fprintf(stderr, "%s: node=%s\n", __FUNCTION__, node);
//	UNUSED(pw);
//	UNUSED(node);
	*nhints = 0;
	*hints = NULL;
	return CSS_OK;
}

static css_error
resolve_url_empty(void *pw, const char *base, lwc_string *rel, lwc_string **abs)
{
	return CSS_OK;
}

css_error
resolve_url(void *pw, const char *base, lwc_string *rel, lwc_string **abs)
{
	lwc_error lerror;

	char *url = straconcat(base, lwc_string_data(rel), NULL);

	if (!url) {
		*abs = NULL;
		return CSS_NOMEM;
	}

	lerror = lwc_intern_string(url, strlen(url), abs);
	if (lerror != lwc_error_ok) {
		*abs = NULL;
		mem_free(url);
		return lerror == lwc_error_oom ? CSS_NOMEM : CSS_INVALID;
	}

	mem_free(url);
	return CSS_OK;
}

/**
 * Create an inline style
 *
 * \param data          Source data
 * \param len           Length of data in bytes
 * \param charset       Charset of data, or NULL if unknown
 * \param url           Base URL of document containing data
 * \param allow_quirks  True to permit CSS parsing quirks
 * \return Pointer to stylesheet, or NULL on failure.
 */
css_stylesheet *nscss_create_inline_style(const uint8_t *data, size_t len,
		const char *charset, const char *url, bool allow_quirks)
{
	css_stylesheet_params params;
	css_stylesheet *sheet;
	css_error error;

	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_DEFAULT;
	params.charset = charset;
	params.url = url;
	params.title = NULL;
	params.allow_quirks = allow_quirks;
	params.inline_style = true;
	params.resolve = resolve_url;
	params.resolve_pw = NULL;
	params.import = NULL;
	params.import_pw = NULL;
	params.color = NULL;//  ns_system_colour;
	params.color_pw = NULL;
	params.font = NULL;
	params.font_pw = NULL;

	error = css_stylesheet_create(&params, &sheet);
	if (error != CSS_OK) {

		fprintf(stderr, "Failed creating sheet: %d", error);
		return NULL;
	}

	error = css_stylesheet_append_data(sheet, data, len);
	if (error != CSS_OK && error != CSS_NEEDDATA) {
		fprintf(stderr, "failed appending data: %d", error);
		css_stylesheet_destroy(sheet);
		return NULL;
	}

	error = css_stylesheet_data_done(sheet);
	if (error != CSS_OK) {
		fprintf(stderr, "failed completing parse: %d", error);
		css_stylesheet_destroy(sheet);
		return NULL;
	}

	return sheet;
}

/**
 * Selection callback table for libcss
 */
static css_select_handler selection_handler = {
	CSS_SELECT_HANDLER_VERSION_1,

	node_name,
	node_classes,
	node_id,
	named_ancestor_node,
	named_parent_node,
	named_sibling_node,
	named_generic_sibling_node,
	parent_node,
	sibling_node,
	node_has_name,
	node_has_class,
	node_has_id,
	node_has_attribute,
	node_has_attribute_equal,
	node_has_attribute_dashmatch,
	node_has_attribute_includes,
	node_has_attribute_prefix,
	node_has_attribute_suffix,
	node_has_attribute_substring,
	node_is_root,
	node_count_siblings,
	node_is_empty,
	node_is_link,
	node_is_visited,
	node_is_hover,
	node_is_active,
	node_is_focus,
	node_is_enabled,
	node_is_disabled,
	node_is_checked,
	node_is_target,
	node_is_lang,
	node_presentational_hint,
	ua_default_for_property,
	set_libcss_node_data,
	get_libcss_node_data
};


/* Handler for libcss_node_data, stored as libdom node user data */
static void
nscss_dom_user_data_handler(dom_node_operation operation,
		dom_string *key, void *data, struct dom_node *src,
		struct dom_node *dst)
{
	css_error error;

	if (dom_string_isequal(corestring_dom___ns_key_libcss_node_data,
			key) == false || data == NULL) {
		return;
	}

	switch (operation) {
	case DOM_NODE_CLONED:
		error = css_libcss_node_data_handler(&selection_handler,
				CSS_NODE_CLONED,
				NULL, src, dst, data);
		if (error != CSS_OK)
			fprintf(stderr,
			      "Failed to clone libcss_node_data.");
		break;

	case DOM_NODE_RENAMED:
		error = css_libcss_node_data_handler(&selection_handler,
				CSS_NODE_MODIFIED,
				NULL, src, NULL, data);
		if (error != CSS_OK)
			fprintf(stderr,
			      "Failed to update libcss_node_data.");
		break;

	case DOM_NODE_IMPORTED:
	case DOM_NODE_ADOPTED:
	case DOM_NODE_DELETED:
		error = css_libcss_node_data_handler(&selection_handler,
				CSS_NODE_DELETED,
				NULL, src, NULL, data);
		if (error != CSS_OK)
			fprintf(stderr,
			      "Failed to delete libcss_node_data.");
		break;

	default:
		fprintf(stderr, "User data operation not handled.");
		assert(0);
	}
}

/**
 * Get style selection results for an element
 *
 * \param ctx             CSS selection context
 * \param n               Element to select for
 * \param media           Permitted media types
 * \param inline_style    Inline style associated with element, or NULL
 * \return Pointer to selection results (containing computed styles),
 *         or NULL on failure
 */
css_select_results *nscss_get_style(nscss_select_ctx *ctx, dom_node *n,
		const css_media *media,
		const css_unit_ctx *unit_len_ctx,
		const css_stylesheet *inline_style)
{
	css_computed_style *composed;
	css_select_results *styles;
	int pseudo_element;
	css_error error;

	/* Select style for node */
	error = css_select_style(ctx->ctx, n, unit_len_ctx, media, inline_style,
			&selection_handler, ctx, &styles);

	if (error != CSS_OK || styles == NULL) {
		/* Failed selecting partial style -- bail out */
		return NULL;
	}

	/* If there's a parent style, compose with partial to obtain
	 * complete computed style for element */
	if (ctx->parent_style != NULL) {
		/* Complete the computed style, by composing with the parent
		 * element's style */
		error = css_computed_style_compose(ctx->parent_style,
				styles->styles[CSS_PSEUDO_ELEMENT_NONE],
				unit_len_ctx, &composed);
		if (error != CSS_OK) {
			css_select_results_destroy(styles);
			return NULL;
		}

		/* Replace select_results style with composed style */
		css_computed_style_destroy(
				styles->styles[CSS_PSEUDO_ELEMENT_NONE]);
		styles->styles[CSS_PSEUDO_ELEMENT_NONE] = composed;
	}

	for (pseudo_element = CSS_PSEUDO_ELEMENT_NONE + 1;
			pseudo_element < CSS_PSEUDO_ELEMENT_COUNT;
			pseudo_element++) {

		if (pseudo_element == CSS_PSEUDO_ELEMENT_FIRST_LETTER ||
				pseudo_element == CSS_PSEUDO_ELEMENT_FIRST_LINE)
			/* TODO: Handle first-line and first-letter pseudo
			 *       element computed style completion */
			continue;

		if (styles->styles[pseudo_element] == NULL)
			/* There were no rules concerning this pseudo element */
			continue;

		/* Complete the pseudo element's computed style, by composing
		 * with the base element's style */
		error = css_computed_style_compose(
				styles->styles[CSS_PSEUDO_ELEMENT_NONE],
				styles->styles[pseudo_element],
				unit_len_ctx, &composed);
		if (error != CSS_OK) {
			/* TODO: perhaps this shouldn't be quite so
			 * catastrophic? */
			css_select_results_destroy(styles);
			return NULL;
		}

		/* Replace select_results style with composed style */
		css_computed_style_destroy(styles->styles[pseudo_element]);
		styles->styles[pseudo_element] = composed;
	}

	return styles;
}

/**
 * Get a blank style
 *
 * \param ctx     CSS selection context
 * \param parent  Parent style to cascade inherited properties from
 * \return Pointer to blank style, or NULL on failure
 */
css_computed_style *nscss_get_blank_style(nscss_select_ctx *ctx,
		const css_unit_ctx *unit_len_ctx,
		const css_computed_style *parent)
{
	css_computed_style *partial, *composed;
	css_error error;

	error = css_select_default_style(ctx->ctx,
			&selection_handler, ctx, &partial);
	if (error != CSS_OK) {
		return NULL;
	}

	/* TODO: Do we really need to compose?  Initial style shouldn't
	 * have any inherited properties. */
	error = css_computed_style_compose(parent, partial,
			unit_len_ctx, &composed);
	css_computed_style_destroy(partial);
	if (error != CSS_OK) {
		css_computed_style_destroy(composed);
		return NULL;
	}

	return composed;
}


/******************************************************************************
 * Style selection callbacks                                                  *
 ******************************************************************************/

/**
 * Callback to retrieve a node's name.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Pointer to location to receive node name
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 */
css_error node_name(void *pw, void *node, css_qname *qname)
{
	dom_node *n = node;
	dom_string *name;
	dom_exception err;

	err = dom_node_get_node_name(n, &name);
	if (err != DOM_NO_ERR)
		return CSS_NOMEM;

	qname->ns = NULL;

	err = dom_string_intern(name, &qname->name);
	if (err != DOM_NO_ERR) {
		dom_string_unref(name);
		return CSS_NOMEM;
	}

	dom_string_unref(name);

	return CSS_OK;
}

/**
 * Callback to retrieve a node's classes.
 *
 * \param pw         HTML document
 * \param node       DOM node
 * \param classes    Pointer to location to receive class name array
 * \param n_classes  Pointer to location to receive length of class name array
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \note The returned array will be destroyed by libcss. Therefore, it must
 *       be allocated using the same allocator as used by libcss during style
 *       selection.
 */
css_error node_classes(void *pw, void *node,
		lwc_string ***classes, uint32_t *n_classes)
{
	dom_node *n = node;
	dom_exception err;

	*classes = NULL;
	*n_classes = 0;

	err = dom_element_get_classes(n, classes, n_classes);
	if (err != DOM_NO_ERR)
		return CSS_NOMEM;

	return CSS_OK;
}

/**
 * Callback to retrieve a node's ID.
 *
 * \param pw    HTML document
 * \param node  DOM node
 * \param id    Pointer to location to receive id value
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 */
css_error node_id(void *pw, void *node, lwc_string **id)
{
	dom_node *n = node;
	dom_string *attr;
	dom_exception err;

	*id = NULL;

	/** \todo Assumes an HTML DOM */
	err = dom_html_element_get_id(n, &attr);
	if (err != DOM_NO_ERR)
		return CSS_NOMEM;

	if (attr != NULL) {
		err = dom_string_intern(attr, id);
		if (err != DOM_NO_ERR) {
			dom_string_unref(attr);
			return CSS_NOMEM;
		}
		dom_string_unref(attr);
	}

	return CSS_OK;
}

/**
 * Callback to find a named ancestor node.
 *
 * \param pw        HTML document
 * \param node      DOM node
 * \param qname     Node name to search for
 * \param ancestor  Pointer to location to receive ancestor
 * \return CSS_OK.
 *
 * \post \a ancestor will contain the result, or NULL if there is no match
 */
css_error named_ancestor_node(void *pw, void *node,
		const css_qname *qname, void **ancestor)
{
	dom_element_named_ancestor_node(node, qname->name,
			(struct dom_element **)ancestor);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(*ancestor);

	return CSS_OK;
}

/**
 * Callback to find a named parent node
 *
 * \param pw      HTML document
 * \param node    DOM node
 * \param qname   Node name to search for
 * \param parent  Pointer to location to receive parent
 * \return CSS_OK.
 *
 * \post \a parent will contain the result, or NULL if there is no match
 */
css_error named_parent_node(void *pw, void *node,
		const css_qname *qname, void **parent)
{
	dom_element_named_parent_node(node, qname->name,
			(struct dom_element **)parent);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(*parent);

	return CSS_OK;
}

/**
 * Callback to find a named sibling node.
 *
 * \param pw       HTML document
 * \param node     DOM node
 * \param qname    Node name to search for
 * \param sibling  Pointer to location to receive sibling
 * \return CSS_OK.
 *
 * \post \a sibling will contain the result, or NULL if there is no match
 */
css_error named_sibling_node(void *pw, void *node,
		const css_qname *qname, void **sibling)
{
	dom_node *n = node;
	dom_node *prev;
	dom_exception err;

	*sibling = NULL;

	/* Find sibling element */
	err = dom_node_get_previous_sibling(n, &n);
	if (err != DOM_NO_ERR)
		return CSS_OK;

	while (n != NULL) {
		dom_node_type type;

		err = dom_node_get_node_type(n, &type);
		if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			return CSS_OK;
		}

		if (type == DOM_ELEMENT_NODE)
			break;

		err = dom_node_get_previous_sibling(n, &prev);
		if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			return CSS_OK;
		}

#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(n);
		n = prev;
	}

	if (n != NULL) {
		dom_string *name;

		err = dom_node_get_node_name(n, &name);
		if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			return CSS_OK;
		}

#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(n);

		if (dom_string_caseless_lwc_isequal(name, qname->name)) {
			*sibling = n;
		}

		dom_string_unref(name);
	}

	return CSS_OK;
}

/**
 * Callback to find a named generic sibling node.
 *
 * \param pw       HTML document
 * \param node     DOM node
 * \param qname    Node name to search for
 * \param sibling  Pointer to location to receive ancestor
 * \return CSS_OK.
 *
 * \post \a sibling will contain the result, or NULL if there is no match
 */
css_error named_generic_sibling_node(void *pw, void *node,
		const css_qname *qname, void **sibling)
{
	dom_node *n = node;
	dom_node *prev;
	dom_exception err;

	*sibling = NULL;

	err = dom_node_get_previous_sibling(n, &n);
	if (err != DOM_NO_ERR)
		return CSS_OK;

	while (n != NULL) {
		dom_node_type type;
		dom_string *name;

		err = dom_node_get_node_type(n, &type);
		if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			return CSS_OK;
		}

		if (type == DOM_ELEMENT_NODE) {
			err = dom_node_get_node_name(n, &name);
			if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
				dom_node_unref(n);
				return CSS_OK;
			}

			if (dom_string_caseless_lwc_isequal(name,
					qname->name)) {
				dom_string_unref(name);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
				dom_node_unref(n);
				*sibling = n;
				break;
			}
			dom_string_unref(name);
		}

		err = dom_node_get_previous_sibling(n, &prev);
		if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			return CSS_OK;
		}

#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(n);
		n = prev;
	}

	return CSS_OK;
}

/**
 * Callback to retrieve the parent of a node.
 *
 * \param pw      HTML document
 * \param node    DOM node
 * \param parent  Pointer to location to receive parent
 * \return CSS_OK.
 *
 * \post \a parent will contain the result, or NULL if there is no match
 */
css_error parent_node(void *pw, void *node, void **parent)
{
	dom_element_parent_node(node, (struct dom_element **)parent);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
	dom_node_unref(*parent);

	return CSS_OK;
}

/**
 * Callback to retrieve the preceding sibling of a node.
 *
 * \param pw       HTML document
 * \param node     DOM node
 * \param sibling  Pointer to location to receive sibling
 * \return CSS_OK.
 *
 * \post \a sibling will contain the result, or NULL if there is no match
 */
css_error sibling_node(void *pw, void *node, void **sibling)
{
	dom_node *n = node;
	dom_node *prev;
	dom_exception err;

	*sibling = NULL;

	/* Find sibling element */
	err = dom_node_get_previous_sibling(n, &n);
	if (err != DOM_NO_ERR)
		return CSS_OK;

	while (n != NULL) {
		dom_node_type type;

		err = dom_node_get_node_type(n, &type);
		if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			return CSS_OK;
		}

		if (type == DOM_ELEMENT_NODE)
			break;

		err = dom_node_get_previous_sibling(n, &prev);
		if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			return CSS_OK;
		}

#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(n);
		n = prev;
	}

	if (n != NULL) {
		/** \todo Sort out reference counting */
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(n);

		*sibling = n;
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has the given name.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_name(void *pw, void *node,
		const css_qname *qname, bool *match)
{
	nscss_select_ctx *ctx = pw;
	dom_node *n = node;

	if (lwc_string_isequal(qname->name, ctx->universal, match) ==
			lwc_error_ok && *match == false) {
		dom_string *name;
		dom_exception err;

		err = dom_node_get_node_name(n, &name);
		if (err != DOM_NO_ERR)
			return CSS_OK;

		/* Element names are case insensitive in HTML */
		*match = dom_string_caseless_lwc_isequal(name, qname->name);

		dom_string_unref(name);
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has the given class.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param name   Name to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_class(void *pw, void *node,
		lwc_string *name, bool *match)
{
	dom_node *n = node;

	/** \todo: Ensure that libdom performs case-insensitive
	 * matching in quirks mode */
	(void)dom_element_has_class(n, name, match);

	return CSS_OK;
}

/**
 * Callback to determine if a node has the given id.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param name   Name to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_id(void *pw, void *node,
		lwc_string *name, bool *match)
{
	dom_node *n = node;
	dom_string *attr;
	dom_exception err;

	*match = false;

	/** \todo Assumes an HTML DOM */
	err = dom_html_element_get_id(n, &attr);
	if (err != DOM_NO_ERR)
		return CSS_OK;

	if (attr != NULL) {
		*match = dom_string_lwc_isequal(attr, name);

		dom_string_unref(attr);
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute(void *pw, void *node,
		const css_qname *qname, bool *match)
{
	dom_node *n = node;
	dom_string *name;
	dom_exception err;

	err = dom_string_create_interned(
			(const uint8_t *) lwc_string_data(qname->name),
			lwc_string_length(qname->name), &name);
	if (err != DOM_NO_ERR)
		return CSS_NOMEM;

	err = dom_element_has_attribute(n, name, match);
	if (err != DOM_NO_ERR) {
		dom_string_unref(name);
		return CSS_OK;
	}

	dom_string_unref(name);

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with given name and value.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_equal(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	dom_node *n = node;
	dom_string *name;
	dom_string *atr_val;
	dom_exception err;

	size_t vlen = lwc_string_length(value);

	if (vlen == 0) {
		*match = false;
		return CSS_OK;
	}

	err = dom_string_create_interned(
		(const uint8_t *) lwc_string_data(qname->name),
		lwc_string_length(qname->name), &name);
	if (err != DOM_NO_ERR)
		return CSS_NOMEM;

	err = dom_element_get_attribute(n, name, &atr_val);
	if ((err != DOM_NO_ERR) || (atr_val == NULL)) {
		dom_string_unref(name);
		*match = false;
		return CSS_OK;
	}

	dom_string_unref(name);

	*match = dom_string_caseless_lwc_isequal(atr_val, value);

	dom_string_unref(atr_val);

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value dashmatches that given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_dashmatch(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	dom_node *n = node;
	dom_string *name;
	dom_string *atr_val;
	dom_exception err;

	size_t vlen = lwc_string_length(value);

	if (vlen == 0) {
		*match = false;
		return CSS_OK;
	}

	err = dom_string_create_interned(
		(const uint8_t *) lwc_string_data(qname->name),
		lwc_string_length(qname->name), &name);
	if (err != DOM_NO_ERR)
		return CSS_NOMEM;

	err = dom_element_get_attribute(n, name, &atr_val);
	if ((err != DOM_NO_ERR) || (atr_val == NULL)) {
		dom_string_unref(name);
		*match = false;
		return CSS_OK;
	}

	dom_string_unref(name);

	/* check for exact match */
	*match = dom_string_caseless_lwc_isequal(atr_val, value);

	/* check for dashmatch */
	if (*match == false) {
		const char *vdata = lwc_string_data(value);
		const char *data = (const char *) dom_string_data(atr_val);
		size_t len = dom_string_byte_length(atr_val);

		if (len > vlen && data[vlen] == '-' &&
		    strncasecmp(data, vdata, vlen) == 0) {
				*match = true;
		}
	}

	dom_string_unref(atr_val);

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value includes that given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_includes(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	dom_node *n = node;
	dom_string *name;
	dom_string *atr_val;
	dom_exception err;
	size_t vlen = lwc_string_length(value);
	const char *p;
	const char *start;
	const char *end;

	*match = false;

	if (vlen == 0) {
		return CSS_OK;
	}

	err = dom_string_create_interned(
		(const uint8_t *) lwc_string_data(qname->name),
		lwc_string_length(qname->name), &name);
	if (err != DOM_NO_ERR)
		return CSS_NOMEM;

	err = dom_element_get_attribute(n, name, &atr_val);
	if ((err != DOM_NO_ERR) || (atr_val == NULL)) {
		dom_string_unref(name);
		*match = false;
		return CSS_OK;
	}

	dom_string_unref(name);

	/* check for match */
	start = (const char *) dom_string_data(atr_val);
	end = start + dom_string_byte_length(atr_val);

	for (p = start; p <= end; p++) {
		if (*p == ' ' || *p == '\0') {
			if ((size_t) (p - start) == vlen &&
			    strncasecmp(start,
					lwc_string_data(value),
					vlen) == 0) {
				*match = true;
				break;
			}

			start = p + 1;
		}
	}

	dom_string_unref(atr_val);

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value has the prefix given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_prefix(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	dom_node *n = node;
	dom_string *name;
	dom_string *atr_val;
	dom_exception err;

	size_t vlen = lwc_string_length(value);

	if (vlen == 0) {
		*match = false;
		return CSS_OK;
	}

	err = dom_string_create_interned(
		(const uint8_t *) lwc_string_data(qname->name),
		lwc_string_length(qname->name), &name);
	if (err != DOM_NO_ERR)
		return CSS_NOMEM;

	err = dom_element_get_attribute(n, name, &atr_val);
	if ((err != DOM_NO_ERR) || (atr_val == NULL)) {
		dom_string_unref(name);
		*match = false;
		return CSS_OK;
	}

	dom_string_unref(name);

	/* check for exact match */
	*match = dom_string_caseless_lwc_isequal(atr_val, value);

	/* check for prefix match */
	if (*match == false) {
		const char *data = (const char *) dom_string_data(atr_val);
		size_t len = dom_string_byte_length(atr_val);

		if ((len >= vlen) &&
		    (strncasecmp(data, lwc_string_data(value), vlen) == 0)) {
			*match = true;
		}
	}

	dom_string_unref(atr_val);

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value has the suffix given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_suffix(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	dom_node *n = node;
	dom_string *name;
	dom_string *atr_val;
	dom_exception err;

	size_t vlen = lwc_string_length(value);

	if (vlen == 0) {
		*match = false;
		return CSS_OK;
	}

	err = dom_string_create_interned(
		(const uint8_t *) lwc_string_data(qname->name),
		lwc_string_length(qname->name), &name);
	if (err != DOM_NO_ERR)
		return CSS_NOMEM;

	err = dom_element_get_attribute(n, name, &atr_val);
	if ((err != DOM_NO_ERR) || (atr_val == NULL)) {
		dom_string_unref(name);
		*match = false;
		return CSS_OK;
	}

	dom_string_unref(name);

	/* check for exact match */
	*match = dom_string_caseless_lwc_isequal(atr_val, value);

	/* check for prefix match */
	if (*match == false) {
		const char *data = (const char *) dom_string_data(atr_val);
		size_t len = dom_string_byte_length(atr_val);

		const char *start = (char *) data + len - vlen;

		if ((len >= vlen) &&
		    (strncasecmp(start, lwc_string_data(value), vlen) == 0)) {
			*match = true;
		}


	}

	dom_string_unref(atr_val);

	return CSS_OK;
}

/**
 * Callback to determine if a node has an attribute with the given name whose
 * value contains the substring given.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param qname  Name to match
 * \param value  Value to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK on success,
 *         CSS_NOMEM on memory exhaustion.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_has_attribute_substring(void *pw, void *node,
		const css_qname *qname, lwc_string *value,
		bool *match)
{
	dom_node *n = node;
	dom_string *name;
	dom_string *atr_val;
	dom_exception err;

	size_t vlen = lwc_string_length(value);

	if (vlen == 0) {
		*match = false;
		return CSS_OK;
	}

	err = dom_string_create_interned(
		(const uint8_t *) lwc_string_data(qname->name),
		lwc_string_length(qname->name), &name);
	if (err != DOM_NO_ERR)
		return CSS_NOMEM;

	err = dom_element_get_attribute(n, name, &atr_val);
	if ((err != DOM_NO_ERR) || (atr_val == NULL)) {
		dom_string_unref(name);
		*match = false;
		return CSS_OK;
	}

	dom_string_unref(name);

	/* check for exact match */
	*match = dom_string_caseless_lwc_isequal(atr_val, value);

	/* check for prefix match */
	if (*match == false) {
		const char *vdata = lwc_string_data(value);
		const char *start = (const char *) dom_string_data(atr_val);
		size_t len = dom_string_byte_length(atr_val);
		const char *last_start = start + len - vlen;

		if (len >= vlen) {
			while (start <= last_start) {
				if (strncasecmp(start, vdata,
						vlen) == 0) {
					*match = true;
					break;
				}

				start++;
			}
		}
	}

	dom_string_unref(atr_val);

	return CSS_OK;
}

/**
 * Callback to determine if a node is the root node of the document.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_root(void *pw, void *node, bool *match)
{
	dom_node *n = node;
	dom_node *parent;
	dom_node_type type;
	dom_exception err;

	err = dom_node_get_parent_node(n, &parent);
	if (err != DOM_NO_ERR) {
		return CSS_NOMEM;
	}

	if (parent != NULL) {
		err = dom_node_get_node_type(parent, &type);
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(parent);

		if (err != DOM_NO_ERR)
			return CSS_NOMEM;

		if (type != DOM_DOCUMENT_NODE) {
			*match = false;
			return CSS_OK;
		}
	}

	*match = true;

	return CSS_OK;
}

static int
node_count_siblings_check(dom_node *node,
			  bool check_name,
			  dom_string *name)
{
	dom_node_type type;
	int ret = 0;
	dom_exception exc;

	if (node == NULL)
		return 0;

	exc = dom_node_get_node_type(node, &type);
	if ((exc != DOM_NO_ERR) || (type != DOM_ELEMENT_NODE)) {
		return 0;
	}

	if (check_name) {
		dom_string *node_name = NULL;
		exc = dom_node_get_node_name(node, &node_name);

		if ((exc == DOM_NO_ERR) && (node_name != NULL)) {

			if (dom_string_caseless_isequal(name,
							node_name)) {
				ret = 1;
			}
			dom_string_unref(node_name);
		}
	} else {
		ret = 1;
	}

	return ret;
}

/**
 * Callback to count a node's siblings.
 *
 * \param pw         HTML document
 * \param n          DOM node
 * \param same_name  Only count siblings with the same name, or all
 * \param after      Count anteceding instead of preceding siblings
 * \param count      Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a count will contain the number of siblings
 */
css_error node_count_siblings(void *pw, void *n, bool same_name,
		bool after, int32_t *count)
{
	int32_t cnt = 0;
	dom_exception exc;
	dom_string *node_name = NULL;

	if (same_name) {
		dom_node *node = n;
		exc = dom_node_get_node_name(node, &node_name);
		if ((exc != DOM_NO_ERR) || (node_name == NULL)) {
			return CSS_NOMEM;
		}
	}

	if (after) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node *node = dom_node_ref(n);
		dom_node *next;

		do {
			exc = dom_node_get_next_sibling(node, &next);
			if ((exc != DOM_NO_ERR))
				break;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(node);
			node = next;

			cnt += node_count_siblings_check(node, same_name, node_name);
		} while (node != NULL);
	} else {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node *node = dom_node_ref(n);
		dom_node *next;

		do {
			exc = dom_node_get_previous_sibling(node, &next);
			if ((exc != DOM_NO_ERR))
				break;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(node);
			node = next;

			cnt += node_count_siblings_check(node, same_name, node_name);

		} while (node != NULL);
	}

	if (node_name != NULL) {
		dom_string_unref(node_name);
	}

	*count = cnt;
	return CSS_OK;
}

/**
 * Callback to determine if a node is empty.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node is empty and false otherwise.
 */
css_error node_is_empty(void *pw, void *node, bool *match)
{
	dom_node *n = node, *next;
	dom_exception err;

	*match = true;

	err = dom_node_get_first_child(n, &n);
	if (err != DOM_NO_ERR) {
		return CSS_BADPARM;
	}

	while (n != NULL) {
		dom_node_type ntype;
		err = dom_node_get_node_type(n, &ntype);
		if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			return CSS_BADPARM;
		}

		if (ntype == DOM_ELEMENT_NODE ||
		    ntype == DOM_TEXT_NODE) {
			*match = false;
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			break;
		}

		err = dom_node_get_next_sibling(n, &next);
		if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			return CSS_BADPARM;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(n);
		n = next;
	}

	return CSS_OK;
}

/**
 * Callback to determine if a node is a linking element.
 *
 * \param pw     HTML document
 * \param n      DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_link(void *pw, void *n, bool *match)
{
	dom_node *node = n;
	dom_exception exc;
	dom_string *node_name = NULL;

	exc = dom_node_get_node_name(node, &node_name);
	if ((exc != DOM_NO_ERR) || (node_name == NULL)) {
		return CSS_NOMEM;
	}

	if (dom_string_caseless_lwc_isequal(node_name, corestring_lwc_a)) {
		bool has_href;
		exc = dom_element_has_attribute(node, corestring_dom_href,
				&has_href);
		if ((exc == DOM_NO_ERR) && (has_href)) {
			*match = true;
		} else {
			*match = false;
		}
	} else {
		*match = false;
	}
	dom_string_unref(node_name);

	return CSS_OK;
}

/**
 * Callback to determine if a node is a linking element whose target has been
 * visited.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_visited(void *pw, void *node, bool *match)
{
#if 0
	nscss_select_ctx *ctx = pw;
	nsurl *url;
	nserror error;
	const struct url_data *data;

	dom_exception exc;
	dom_node *n = node;
	dom_string *s = NULL;

	*match = false;

	exc = dom_node_get_node_name(n, &s);
	if ((exc != DOM_NO_ERR) || (s == NULL)) {
		return CSS_NOMEM;
	}

	if (!dom_string_caseless_lwc_isequal(s, corestring_lwc_a)) {
		/* Can't be visited; not ancher element */
		dom_string_unref(s);
		return CSS_OK;
	}

	/* Finished with node name string */
	dom_string_unref(s);
	s = NULL;

	exc = dom_element_get_attribute(n, corestring_dom_href, &s);
	if ((exc != DOM_NO_ERR) || (s == NULL)) {
		/* Can't be visited; not got a URL */
		return CSS_OK;
	}

	/* Make href absolute */
	/* TODO: this duplicates what we do for box->href
	 *       should we put the absolute URL on the dom node? */
	error = nsurl_join(ctx->base_url, dom_string_data(s), &url);

	/* Finished with href string */
	dom_string_unref(s);

	if (error != NSERROR_OK) {
		/* Couldn't make nsurl object */
		return CSS_NOMEM;
	}

	data = urldb_get_url_data(url);

	/* Visited if in the db and has
	 * non-zero visit count */
	if (data != NULL && data->visits > 0)
		*match = true;

	nsurl_unref(url);
#endif
	*match = false;
	return CSS_OK;
}

/**
 * Callback to determine if a node is currently being hovered over.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_hover(void *pw, void *node, bool *match)
{
	/** \todo Support hovering */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node is currently activated.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_active(void *pw, void *node, bool *match)
{
	/** \todo Support active nodes */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node has the input focus.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_focus(void *pw, void *node, bool *match)
{
	/** \todo Support focussed nodes */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node is enabled.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match with contain true if the node is enabled and false otherwise.
 */
css_error node_is_enabled(void *pw, void *node, bool *match)
{
	/** \todo Support enabled nodes */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node is disabled.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match with contain true if the node is disabled and false otherwise.
 */
css_error node_is_disabled(void *pw, void *node, bool *match)
{
	/** \todo Support disabled nodes */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node is checked.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match with contain true if the node is checked and false otherwise.
 */
css_error node_is_checked(void *pw, void *node, bool *match)
{
	/** \todo Support checked nodes */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node is the target of the document URL.
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match with contain true if the node matches and false otherwise.
 */
css_error node_is_target(void *pw, void *node, bool *match)
{
	/** \todo Support target */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to determine if a node has the given language
 *
 * \param pw     HTML document
 * \param node   DOM node
 * \param lang   Language specifier to match
 * \param match  Pointer to location to receive result
 * \return CSS_OK.
 *
 * \post \a match will contain true if the node matches and false otherwise.
 */
css_error node_is_lang(void *pw, void *node,
		lwc_string *lang, bool *match)
{
	/** \todo Support languages */

	*match = false;

	return CSS_OK;
}

/**
 * Callback to retrieve the User-Agent defaults for a CSS property.
 *
 * \param pw        HTML document
 * \param property  Property to retrieve defaults for
 * \param hint      Pointer to hint object to populate
 * \return CSS_OK       on success,
 *         CSS_INVALID  if the property should not have a user-agent default.
 */
css_error ua_default_for_property(void *pw, uint32_t property, css_hint *hint)
{
	UNUSED(pw);

	if (property == CSS_PROP_COLOR) {
		hint->data.color = 0x00bfbfbf;
		hint->status = CSS_COLOR_COLOR;
	} else if (property == CSS_PROP_FONT_FAMILY) {
		hint->data.strings = NULL;
		hint->status = CSS_FONT_FAMILY_SANS_SERIF;
	} else if (property == CSS_PROP_QUOTES) {
		/* Not exactly useful :) */
		hint->data.strings = NULL;
		hint->status = CSS_QUOTES_NONE;
	} else if (property == CSS_PROP_VOICE_FAMILY) {
		/** \todo Fix this when we have voice-family done */
		hint->data.strings = NULL;
		hint->status = 0;
	} else {
		return CSS_INVALID;
	}

	return CSS_OK;
}

css_error
set_libcss_node_data(void *pw, void *node, void *libcss_node_data)
{
	dom_node *n = node;
	dom_exception err;
	void *old_node_data = NULL;

	/* Set this node's node data */
	err = dom_node_set_user_data(n,
			corestring_dom___ns_key_libcss_node_data,
			libcss_node_data, nscss_dom_user_data_handler,
			(void *) &old_node_data);
	if (err != DOM_NO_ERR) {
		return CSS_NOMEM;
	}
	//assert(old_node_data == NULL);

	return CSS_OK;
}

css_error
get_libcss_node_data(void *pw, void *node, void **libcss_node_data)
{
	dom_node *n = node;
	dom_exception err;

	/* Get this node's node data */
	err = dom_node_get_user_data(n,
			corestring_dom___ns_key_libcss_node_data,
			libcss_node_data);
	if (err != DOM_NO_ERR) {
		return CSS_NOMEM;
	}

	return CSS_OK;
}

static void
apply_color(struct html_context *html_context, struct html_element *html_element, css_color color_shade)
{
	if (use_document_fg_colors(html_context->options)) {
		html_element->attr.style.color.foreground = color_shade & 0x00ffffff;
	}
}

static void
apply_background_color(struct html_context *html_context, struct html_element *html_element, css_color color_shade)
{
	if (use_document_bg_colors(html_context->options)) {
		html_element->attr.style.color.background = color_shade & 0x00ffffff;
	}
}

static void
apply_font_attribute(struct html_context *html_context,
			 struct html_element *element, bool underline, bool bold, bool strike)
{
	int add = 0;
	int rem = 0;
	if (underline) {
		add |= AT_UNDERLINE;
	} else {
		rem |= AT_UNDERLINE;
	}

	if (bold) {
		add |= AT_BOLD;
	} else {
		rem |= AT_BOLD;
	}

	if (strike) {
		add |= AT_STRIKE;
	} else {
		rem |= AT_STRIKE;
	}

	element->attr.style.attr |= add;
	element->attr.style.attr &= ~rem;
}

static void
apply_list_style(struct html_context *html_context, struct html_element *element, uint8_t list_type)
{
	element->parattr.list_number = 1;

	switch (list_type) {
	case CSS_LIST_STYLE_TYPE_DISC:
		element->parattr.list_number = 0;
		element->parattr.flags = P_DISC;
		break;
	case CSS_LIST_STYLE_TYPE_CIRCLE:
		element->parattr.list_number = 0;
		element->parattr.flags = P_O;
		break;
	case CSS_LIST_STYLE_TYPE_SQUARE:
		element->parattr.list_number = 0;
		element->parattr.flags = P_SQUARE;
		break;
	case CSS_LIST_STYLE_TYPE_DECIMAL:
		element->parattr.flags = P_NUMBER;
		break;
	case CSS_LIST_STYLE_TYPE_DECIMAL_LEADING_ZERO:
		element->parattr.flags = P_NUMBER;
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_ALPHA:
		element->parattr.flags = P_alpha;
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_ROMAN:
		element->parattr.flags = P_roman;
		break;
	case CSS_LIST_STYLE_TYPE_UPPER_ALPHA:
		element->parattr.flags = P_ALPHA;
		break;
	case CSS_LIST_STYLE_TYPE_UPPER_ROMAN:
		element->parattr.flags = P_ROMAN;
		break;
	case CSS_LIST_STYLE_TYPE_NONE:
		element->parattr.flags = P_NO_BULLET;
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_LATIN:
		element->parattr.flags = P_alpha;
		break;
	case CSS_LIST_STYLE_TYPE_UPPER_LATIN:
		element->parattr.flags = P_ALPHA;
		break;
	case CSS_LIST_STYLE_TYPE_ARMENIAN:
		element->parattr.flags = P_NUMBER;
		break;
	case CSS_LIST_STYLE_TYPE_GEORGIAN:
		element->parattr.flags = P_NUMBER;
		break;
	case CSS_LIST_STYLE_TYPE_LOWER_GREEK:
		element->parattr.flags = P_NUMBER;
		break;
	default:
		element->parattr.list_number = 0;
		break;
	}
}

static void
apply_display(struct html_context *html_context, struct html_element *element, uint8_t display)
{
	switch (display) {
		case CSS_DISPLAY_INLINE:
//			element->linebreak = 0;
			break;
		case CSS_DISPLAY_BLOCK:
			/* 1 or 2, that is the question. I went for 2 since it
			 * gives a more "blocky" feel and it's more common.
			 * YMMV. */
			element->linebreak = 2;
			break;
		case CSS_DISPLAY_NONE:
			if (!html_context->options->css_ignore_display_none)
				element->invisible = 2;
			break;
		default:
			//INTERNAL("Bad prop->value.display %d", prop->value.display);
			break;
	}
}

static void
apply_text_align(struct html_context *html_context, struct html_element *element, uint8_t text_align)
{
	switch (text_align) {
	case CSS_TEXT_ALIGN_LEFT:
		element->parattr.align = ALIGN_LEFT;
		break;
	case CSS_TEXT_ALIGN_RIGHT:
		element->parattr.align = ALIGN_RIGHT;
		break;
	case CSS_TEXT_ALIGN_CENTER:
		element->parattr.align = ALIGN_CENTER;
		break;
	case CSS_TEXT_ALIGN_JUSTIFY:
		element->parattr.align = ALIGN_JUSTIFY;
		break;
	default:
		element->parattr.align = 0;
		break;
	}
}

static void
apply_font_style(struct html_context *html_context, struct html_element *element, uint8_t font_style)
{
	int add = 0;
	int rem = 0;

	switch (font_style) {
	case CSS_FONT_STYLE_NORMAL:
		rem |= AT_ITALIC;
		break;
	case CSS_FONT_STYLE_ITALIC:
	case CSS_FONT_STYLE_OBLIQUE:
		add |= AT_ITALIC;
	default:
		break;
	}

	element->attr.style.attr |= add;
	element->attr.style.attr &= ~rem;
}

static bool
is_bold(int val)
{
	switch (val) {
	case CSS_FONT_WEIGHT_100:
	case CSS_FONT_WEIGHT_200:
	case CSS_FONT_WEIGHT_300:
	case CSS_FONT_WEIGHT_400:
	case CSS_FONT_WEIGHT_NORMAL:
	case CSS_FONT_WEIGHT_500:
	case CSS_FONT_WEIGHT_600:
	default:
		return false;

	case CSS_FONT_WEIGHT_700:
	case CSS_FONT_WEIGHT_BOLD:
	case CSS_FONT_WEIGHT_800:
	case CSS_FONT_WEIGHT_900:
		return true;
	}
}


void
select_css(struct html_context *html_context, struct html_element *html_element)
{
	css_error code;
	uint8_t color_type;
	css_color color_shade;
	css_select_results *style;
	css_stylesheet *inline_style = NULL;
	dom_document *doc = NULL; /* document, loaded into libdom */
	dom_node *root = NULL; /* root element of document */
	dom_exception exc;

	css_media media = {
		.type = CSS_MEDIA_SCREEN,
	};
	css_unit_ctx unit_len_ctx = {0};
	unit_len_ctx.viewport_width  = 800; // TODO
	unit_len_ctx.viewport_height = 600; // TODO
	unit_len_ctx.device_dpi = F_90; //device_dpi;

	/** \todo Change nsoption font sizes to px. */
///	f_size = FDIV(FMUL(F_96, FDIV(INTTOFIX(nsoption_int(font_size)), F_10)), F_72);
///	f_min  = FDIV(FMUL(F_96, FDIV(INTTOFIX(nsoption_int(font_min_size)), F_10)), F_72);

	unsigned int f_size = FDIV(FMUL(F_96, FDIV(INTTOFIX(50), F_10)), F_72); // TODO
	unsigned int f_min  = FDIV(FMUL(F_96, FDIV(INTTOFIX(50), F_10)), F_72); // TODO

	unit_len_ctx.font_size_default = f_size;
	unit_len_ctx.font_size_minimum = f_min;

	int offset = html_element->name - html_context->document->text.source;
	dom_node *el = (dom_node *)find_in_map(html_context->document->element_map, offset);

	if (!el) {
		return;
	}
	dom_string *s;
	dom_exception err;
	nscss_select_ctx ctx = {0};

	/* Firstly, construct inline stylesheet, if any */
	err = dom_element_get_attribute(el, corestring_dom_style, &s);
	if (err != DOM_NO_ERR)
		return;

	if (s != NULL) {
		inline_style = nscss_create_inline_style(
				(const uint8_t *) dom_string_data(s),
				dom_string_byte_length(s),
				"utf-8",
				"",
				false);

		dom_string_unref(s);

		if (inline_style == NULL)
			return;
	}
	/* Populate selection context */
	ctx.ctx = html_context->select_ctx;
	ctx.quirks = false; //(c->quirks == DOM_DOCUMENT_QUIRKS_MODE_FULL);

//	ctx.base_url = c->base_url;
//	ctx.universal = c->universal;
///	ctx.root_style = root_style;
///	ctx.parent_style = parent_style;

	/* Select style for element */
	style = nscss_get_style(&ctx, el, &media, &unit_len_ctx, inline_style);

	/* No longer need inline style */
	if (inline_style != NULL) {
		css_stylesheet_destroy(inline_style);
	}

	if (!style) {
		return;
	}

	if (!style->styles[CSS_PSEUDO_ELEMENT_NONE]) {
		goto end;
	}

	color_type = css_computed_color(
		style->styles[CSS_PSEUDO_ELEMENT_NONE],
		&color_shade);

	if (color_type) {
		apply_color(html_context, html_element, color_shade);
	}

	color_type = css_computed_background_color(
		style->styles[CSS_PSEUDO_ELEMENT_NONE],
		&color_shade);
	if (color_shade) {
		apply_background_color(html_context, html_element, color_shade);
	}

	bool underline = css_computed_text_decoration(style->styles[CSS_PSEUDO_ELEMENT_NONE]) & CSS_TEXT_DECORATION_UNDERLINE;
	bool strike = css_computed_text_decoration(style->styles[CSS_PSEUDO_ELEMENT_NONE]) & CSS_TEXT_DECORATION_LINE_THROUGH;
	bool bold = is_bold(css_computed_font_weight(style->styles[CSS_PSEUDO_ELEMENT_NONE]));

	apply_font_attribute(html_context, html_element, underline, bold, strike);

	uint8_t font_style = css_computed_font_style(style->styles[CSS_PSEUDO_ELEMENT_NONE]);

	if (font_style) {
		apply_font_style(html_context, html_element, font_style);
	}

	uint8_t list_type = css_computed_list_style_type(style->styles[CSS_PSEUDO_ELEMENT_NONE]);

	if (list_type) {
		apply_list_style(html_context, html_element, list_type);
	}
	doc = html_context->document->dom;
	/* Get root element */
	exc = dom_document_get_document_element(doc, &root);
	if (exc != DOM_NO_ERR) {
		fprintf(stderr, "Exception raised for get_document_element\n");
		//dom_node_unref(doc);
	} else if (root == NULL) {
		fprintf(stderr, "Broken: root == NULL\n");
		//dom_node_unref(doc);
	}

	bool is_root = (root == el);

	if (root) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(root);
	}

	uint8_t display = css_computed_display(style->styles[CSS_PSEUDO_ELEMENT_NONE], is_root);

	if (display) {
		apply_display(html_context, html_element, display);
	}

	uint8_t text_align = css_computed_text_align(style->styles[CSS_PSEUDO_ELEMENT_NONE]);

	if (text_align) {
		apply_text_align(html_context, html_element, text_align);
	}

end:
	code = css_select_results_destroy(style);
	if (code != CSS_OK) {
		fprintf(stderr, "css_computed_style_destroy code=%d\n", code);
	}
}

static css_error
handle_import(void *pw, css_stylesheet *parent, lwc_string *url)
{
	struct html_context *html_context = (struct html_context *)pw;
	char *uristring = memacpy(lwc_string_data(url), lwc_string_length(url));
	struct uri *uri;

	if (!uristring) {
		return CSS_NOMEM;
	}

	uri = get_uri(uristring, URI_BASE);

	if (!uri) {
		mem_free(uristring);
		return CSS_NOMEM;
	}

	/* Request the imported stylesheet as part of the document ... */
	html_context->special_f(html_context, SP_STYLESHEET, uri);

	/* ... and then attempt to import from the cache. */
	import_css2(html_context, uri);

	done_uri(uri);
	mem_free(uristring);

	return CSS_OK;
}


static void
parse_css_common(struct html_context *html_context, const char *text, int length, struct uri *uri)
{
	css_error code;
	size_t size;
	uint32_t count;
	css_stylesheet_params params;
	css_stylesheet *sheet;

	static char url[4096];

	if (uri) {
		char *u = get_uri_string(uri, URI_BASE);

		if (u) {
			char *slash = strrchr(u, '/');

			if (slash) {
				slash[1] = '\0';
			}

			strncpy(url, u, 4095);
			mem_free(u);
		}
	} else {
		url[0] = '\0';
	}

	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_21;
	params.charset = "UTF-8";
	params.url = url;
	params.title = NULL;
	params.allow_quirks = false;
	params.inline_style = false;
	params.resolve = resolve_url;
	params.resolve_pw = NULL;
	params.import = handle_import;
	params.import_pw = html_context;
	params.color = NULL;
	params.color_pw = NULL;
	params.font = NULL;
	params.font_pw = NULL;

	/* create a stylesheet */
	code = css_stylesheet_create(&params, &sheet);
	if (code != CSS_OK) {
		fprintf(stderr, "css_stylesheet_create code=%d\n", code);
		return;
	}
	code = css_stylesheet_append_data(sheet, (const uint8_t *) text, length);

	if (code != CSS_OK && code != CSS_NEEDDATA) {
		fprintf(stderr, "css_stylesheet_append_data code=%d\n", code);
		return;
	}
	code = css_stylesheet_data_done(sheet);
	if (code != CSS_OK && code != CSS_IMPORTS_PENDING) {
		fprintf(stderr, "css_stylesheet_data_done code=%d\n", code);
		return;
	}
	code = css_stylesheet_size(sheet, &size);
	code = css_select_ctx_append_sheet(html_context->select_ctx, sheet, CSS_ORIGIN_AUTHOR,
			NULL);
	if (code != CSS_OK) {
		fprintf(stderr, "css_select_ctx_append_sheet code=%d\n", code);
		return;
	}
	struct el_sheet *el_sheet = (struct el_sheet *)mem_alloc(sizeof(*el_sheet));
	if (el_sheet) {
		el_sheet->sheet = sheet;
		add_to_list(html_context->sheets, el_sheet);
	}
	code = css_select_ctx_count_sheets(html_context->select_ctx, &count);
	if (code != CSS_OK) {
		fprintf(stderr, "css_select_ctx_count_sheets code=%d\n", code);
	}
}

void
parse_css(struct html_context *html_context, char *name)
{
	int offset = name - html_context->document->text.source;
	dom_node *el = (dom_node *)find_in_map(html_context->document->element_map, offset);
	dom_node *n, *next;
	dom_exception err;

	if (!el) {
		return;
	}
	struct string buf;

	if (!init_string(&buf)) {
		return;
	}

	n = el;

	err = dom_node_get_first_child(n, &n);
	if (err != DOM_NO_ERR) {
		goto end;
	}

	while (n != NULL) {
		dom_node_type ntype;
		err = dom_node_get_node_type(n, &ntype);
		if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			goto end;
		}

		if (ntype == DOM_TEXT_NODE) {
			dom_string *str;
			err = dom_node_get_text_content(n, &str);

			if (err == DOM_NO_ERR && str != NULL) {
				int length = dom_string_byte_length(str);
				const char *string_text = dom_string_data(str);

				if (!((length == 1) && (*string_text == '\n'))) {
					add_bytes_to_string(&buf, string_text, length);
				}
				dom_string_unref(str);
			}
		}

		err = dom_node_get_next_sibling(n, &next);
		if (err != DOM_NO_ERR) {
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
			dom_node_unref(n);
			goto end;
		}
#ifdef ECMASCRIPT_DEBUG
fprintf(stderr, "Before: %s:%d\n", __FUNCTION__, __LINE__);
#endif
		dom_node_unref(n);
		n = next;
	}
	parse_css_common(html_context, buf.source, buf.length, NULL);
end:
	done_string(&buf);
}

void
import_css2(struct html_context *html_context, struct uri *uri)
{
	/* Do we have it in the cache? (TODO: CSS cache) */
	struct cache_entry *cached;
	struct fragment *fragment;

	if (!uri) { //|| css->import_level >= MAX_REDIRECTS)
		return;
	}
	cached = get_redirected_cache_entry(uri);

	if (!cached) return;

	fragment = get_cache_fragment(cached);
	if (fragment) {
//		css->import_level++;
		parse_css_common(html_context, fragment->data, fragment->length, uri);
//		css_parse_stylesheet(css, uri, fragment->data, end);
//		css->import_level--;
	}
}

void *
el_match_selector(const char *selector, void *node)
{
	struct string text;
	css_error code;
	size_t size;
	uint32_t count;
	css_stylesheet_params params;
	css_stylesheet *sheet = NULL;
	css_select_ctx *select_ctx = NULL;
	css_select_results *style = NULL;
	uint8_t color_type;
	css_color color_shade;

	css_media media = {
		.type = CSS_MEDIA_SCREEN,
	};

	css_unit_ctx unit_len_ctx = {0};
	unit_len_ctx.viewport_width  = 800; // TODO
	unit_len_ctx.viewport_height = 600; // TODO
	unit_len_ctx.device_dpi = F_90; //device_dpi;

	/** \todo Change nsoption font sizes to px. */
///	f_size = FDIV(FMUL(F_96, FDIV(INTTOFIX(nsoption_int(font_size)), F_10)), F_72);
///	f_min  = FDIV(FMUL(F_96, FDIV(INTTOFIX(nsoption_int(font_min_size)), F_10)), F_72);

	unsigned int f_size = FDIV(FMUL(F_96, FDIV(INTTOFIX(50), F_10)), F_72); // TODO
	unsigned int f_min  = FDIV(FMUL(F_96, FDIV(INTTOFIX(50), F_10)), F_72); // TODO

	unit_len_ctx.font_size_default = f_size;
	unit_len_ctx.font_size_minimum = f_min;
	void *ret = NULL;

	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_21;
	params.charset = "UTF-8";
	params.url = "foo";
	params.title = "foo";
	params.allow_quirks = false;
	params.inline_style = false;
	params.resolve = resolve_url_empty;
	params.resolve_pw = NULL;
	params.import = NULL;
	params.import_pw = NULL;
	params.color = NULL;
	params.color_pw = NULL;
	params.font = NULL;
	params.font_pw = NULL;

	if (!selector) {
		return NULL;
	}
	if (!init_string(&text)) {
		return NULL;
	}
	add_to_string(&text, selector);
	add_to_string(&text, "{color:#123456}");

	/* create a stylesheet */
	code = css_stylesheet_create(&params, &sheet);

	if (code != CSS_OK) {
		goto empty;
	}
	code = css_stylesheet_append_data(sheet, (const uint8_t *) text.source, text.length);

	if (code != CSS_OK && code != CSS_NEEDDATA) {
		goto empty;
	}
	code = css_stylesheet_data_done(sheet);

	if (code != CSS_OK) {
		goto empty;
	}
	//code = css_stylesheet_size(sheet, &size);

	/* prepare a selection context containing the stylesheet */
	code = css_select_ctx_create(&select_ctx);

	if (code != CSS_OK) {
		goto empty;
	}
	code = css_select_ctx_append_sheet(select_ctx, sheet, CSS_ORIGIN_AUTHOR, NULL);

	if (code != CSS_OK) {
		goto empty;
	}
//	code = css_select_ctx_count_sheets(select_ctx, &count);
//
//	if (code != CSS_OK) {
//		goto empty;
//	}
	code = css_select_style(select_ctx, node, &unit_len_ctx, &media, NULL, &selection_handler, 0, &style);

	if (code != CSS_OK) {
		goto empty;
	}
	color_type = css_computed_color(style->styles[CSS_PSEUDO_ELEMENT_NONE], &color_shade);

	if (color_type && color_shade == 0xff123456) {
		ret = node;
	}

empty:

	if (style) {
		css_select_results_destroy(style);
	}

	if (select_ctx) {
		css_select_ctx_destroy(select_ctx);
	}

	if (sheet) {
		css_stylesheet_destroy(sheet);
	}
	done_string(&text);

	return ret;
}
