/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <assert.h>
#include <string.h>
#include <strings.h>

#include "utils/nsoption.h"
#include "utils/corestrings.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "netsurf/plot_style.h"
#include "netsurf/url_db.h"
#include "desktop/system_colour.h"

#include "css/internal.h"
#include "css/hints.h"
#include "css/select.h"

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

static css_error nscss_compute_font_size(void *pw, const css_hint *parent,
		css_hint *size);


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
	nscss_compute_font_size,
	set_libcss_node_data,
	get_libcss_node_data
};

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
	params.resolve = nscss_resolve_url;
	params.resolve_pw = NULL;
	params.import = NULL;
	params.import_pw = NULL;
	params.color = ns_system_colour;
	params.color_pw = NULL;
	params.font = NULL;
	params.font_pw = NULL;

	error = css_stylesheet_create(&params, &sheet);
	if (error != CSS_OK) {
		NSLOG(netsurf, INFO, "Failed creating sheet: %d", error);
		return NULL;
	}

	error = css_stylesheet_append_data(sheet, data, len);
	if (error != CSS_OK && error != CSS_NEEDDATA) {
		NSLOG(netsurf, INFO, "failed appending data: %d", error);
		css_stylesheet_destroy(sheet);
		return NULL;
	}

	error = css_stylesheet_data_done(sheet);
	if (error != CSS_OK) {
		NSLOG(netsurf, INFO, "failed completing parse: %d", error);
		css_stylesheet_destroy(sheet);
		return NULL;
	}

	return sheet;
}

/* Handler for libcss_node_data, stored as libdom node user data */
static void nscss_dom_user_data_handler(dom_node_operation operation,
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
			NSLOG(netsurf, INFO,
			      "Failed to clone libcss_node_data.");
		break;

	case DOM_NODE_RENAMED:
		error = css_libcss_node_data_handler(&selection_handler,
				CSS_NODE_MODIFIED,
				NULL, src, NULL, data);
		if (error != CSS_OK)
			NSLOG(netsurf, INFO,
			      "Failed to update libcss_node_data.");
		break;

	case DOM_NODE_IMPORTED:
	case DOM_NODE_ADOPTED:
	case DOM_NODE_DELETED:
		error = css_libcss_node_data_handler(&selection_handler,
				CSS_NODE_DELETED,
				NULL, src, NULL, data);
		if (error != CSS_OK)
			NSLOG(netsurf, INFO,
			      "Failed to delete libcss_node_data.");
		break;

	default:
		NSLOG(netsurf, INFO, "User data operation not handled.");
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
		const css_media *media, const css_stylesheet *inline_style)
{
	css_computed_style *composed;
	css_select_results *styles;
	int pseudo_element;
	css_error error;

	/* Select style for node */
	error = css_select_style(ctx->ctx, n, media, inline_style,
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
				nscss_compute_font_size, ctx,
				&composed);
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
				nscss_compute_font_size, ctx,
				&composed);
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
			nscss_compute_font_size, ctx, &composed);
	css_computed_style_destroy(partial);
	if (error != CSS_OK) {
		css_computed_style_destroy(composed);
		return NULL;
	}

	return composed;
}

/**
 * Font size computation callback for libcss
 *
 * \param pw      Computation context
 * \param parent  Parent font size (absolute)
 * \param size    Font size to compute
 * \return CSS_OK on success
 *
 * \post \a size will be an absolute font size
 */
css_error nscss_compute_font_size(void *pw, const css_hint *parent,
		css_hint *size)
{
	/**
	 * Table of font-size keyword scale factors
	 *
	 * These are multiplied by the configured default font size
	 * to produce an absolute size for the relevant keyword
	 */
	static const css_fixed factors[] = {
		FLTTOFIX(0.5625), /* xx-small */
		FLTTOFIX(0.6250), /* x-small */
		FLTTOFIX(0.8125), /* small */
		FLTTOFIX(1.0000), /* medium */
		FLTTOFIX(1.1250), /* large */
		FLTTOFIX(1.5000), /* x-large */
		FLTTOFIX(2.0000)  /* xx-large */
	};
	css_hint_length parent_size;

	/* Grab parent size, defaulting to medium if none */
	if (parent == NULL) {
		parent_size.value = FDIV(FMUL(factors[CSS_FONT_SIZE_MEDIUM - 1],
				INTTOFIX(nsoption_int(font_size))),
				INTTOFIX(10));
		parent_size.unit = CSS_UNIT_PT;
	} else {
		assert(parent->status == CSS_FONT_SIZE_DIMENSION);
		assert(parent->data.length.unit != CSS_UNIT_EM);
		assert(parent->data.length.unit != CSS_UNIT_EX);
		assert(parent->data.length.unit != CSS_UNIT_PCT);

		parent_size = parent->data.length;
	}

	assert(size->status != CSS_FONT_SIZE_INHERIT);

	if (size->status < CSS_FONT_SIZE_LARGER) {
		/* Keyword -- simple */
		size->data.length.value = FDIV(FMUL(factors[size->status - 1],
				INTTOFIX(nsoption_int(font_size))), F_10);
		size->data.length.unit = CSS_UNIT_PT;
	} else if (size->status == CSS_FONT_SIZE_LARGER) {
		/** \todo Step within table, if appropriate */
		size->data.length.value =
				FMUL(parent_size.value, FLTTOFIX(1.2));
		size->data.length.unit = parent_size.unit;
	} else if (size->status == CSS_FONT_SIZE_SMALLER) {
		/** \todo Step within table, if appropriate */
		size->data.length.value =
				FDIV(parent_size.value, FLTTOFIX(1.2));
		size->data.length.unit = parent_size.unit;
	} else if (size->data.length.unit == CSS_UNIT_EM ||
			size->data.length.unit == CSS_UNIT_EX ||
			size->data.length.unit == CSS_UNIT_CAP ||
			size->data.length.unit == CSS_UNIT_CH ||
			size->data.length.unit == CSS_UNIT_IC) {
		size->data.length.value =
			FMUL(size->data.length.value, parent_size.value);

		switch (size->data.length.unit) {
		case CSS_UNIT_EX:
			/* 1ex = 0.6em in NetSurf */
			size->data.length.value = FMUL(size->data.length.value,
					FLTTOFIX(0.6));
			break;
		case CSS_UNIT_CAP:
			/* Height of captals.  1cap = 0.9em in NetSurf. */
			size->data.length.value = FMUL(size->data.length.value,
					FLTTOFIX(0.9));
			break;
		case CSS_UNIT_CH:
			/* Width of '0'.  1ch = 0.4em in NetSurf. */
			size->data.length.value = FMUL(size->data.length.value,
					FLTTOFIX(0.4));
			break;
		case CSS_UNIT_IC:
			/* Width of U+6C43.  1ic = 1.1em in NetSurf. */
			size->data.length.value = FMUL(size->data.length.value,
					FLTTOFIX(1.1));
			break;
		default:
			/* No scaling required for EM. */
			break;
		}

		size->data.length.unit = parent_size.unit;
	} else if (size->data.length.unit == CSS_UNIT_PCT) {
		size->data.length.value = FDIV(FMUL(size->data.length.value,
				parent_size.value), INTTOFIX(100));
		size->data.length.unit = parent_size.unit;
	} else if (size->data.length.unit == CSS_UNIT_REM) {
		nscss_select_ctx *ctx = pw;
		if (parent == NULL) {
			size->data.length.value = parent_size.value;
			size->data.length.unit = parent_size.unit;
		} else {
			css_computed_font_size(ctx->root_style,
					&parent_size.value,
					&size->data.length.unit);
			size->data.length.value = FMUL(
					size->data.length.value,
					parent_size.value);
		}
	} else if (size->data.length.unit == CSS_UNIT_RLH) {
		/** TODO: Convert root element line-height to absolute value. */
		size->data.length.value = FMUL(size->data.length.value, FDIV(
				INTTOFIX(nsoption_int(font_size)),
				INTTOFIX(10)));
		size->data.length.unit = CSS_UNIT_PT;
	}

	size->status = CSS_FONT_SIZE_DIMENSION;

	return CSS_OK;
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
			dom_node_unref(n);
			return CSS_OK;
		}

		if (type == DOM_ELEMENT_NODE)
			break;

		err = dom_node_get_previous_sibling(n, &prev);
		if (err != DOM_NO_ERR) {
			dom_node_unref(n);
			return CSS_OK;
		}

		dom_node_unref(n);
		n = prev;
	}

	if (n != NULL) {
		dom_string *name;

		err = dom_node_get_node_name(n, &name);
		if (err != DOM_NO_ERR) {
			dom_node_unref(n);
			return CSS_OK;
		}

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
			dom_node_unref(n);
			return CSS_OK;
		}

		if (type == DOM_ELEMENT_NODE) {
			err = dom_node_get_node_name(n, &name);
			if (err != DOM_NO_ERR) {
				dom_node_unref(n);
				return CSS_OK;
			}

			if (dom_string_caseless_lwc_isequal(name,
					qname->name)) {
				dom_string_unref(name);
				dom_node_unref(n);
				*sibling = n;
				break;
			}
			dom_string_unref(name);
		}

		err = dom_node_get_previous_sibling(n, &prev);
		if (err != DOM_NO_ERR) {
			dom_node_unref(n);
			return CSS_OK;
		}

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
			dom_node_unref(n);
			return CSS_OK;
		}

		if (type == DOM_ELEMENT_NODE)
			break;

		err = dom_node_get_previous_sibling(n, &prev);
		if (err != DOM_NO_ERR) {
			dom_node_unref(n);
			return CSS_OK;
		}

		dom_node_unref(n);
		n = prev;
	}

	if (n != NULL) {
		/** \todo Sort out reference counting */
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
	dom_exception err;

	/** \todo: Ensure that libdom performs case-insensitive
	 * matching in quirks mode */
	err = dom_element_has_class(n, name, match);

	assert(err == DOM_NO_ERR);

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
		dom_node *node = dom_node_ref(n);
		dom_node *next;

		do {
			exc = dom_node_get_next_sibling(node, &next);
			if ((exc != DOM_NO_ERR))
				break;

			dom_node_unref(node);
			node = next;

			cnt += node_count_siblings_check(node, same_name, node_name);
		} while (node != NULL);
	} else {
		dom_node *node = dom_node_ref(n);
		dom_node *next;

		do {
			exc = dom_node_get_previous_sibling(node, &next);
			if ((exc != DOM_NO_ERR))
				break;

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
			dom_node_unref(n);
			return CSS_BADPARM;
		}

		if (ntype == DOM_ELEMENT_NODE ||
		    ntype == DOM_TEXT_NODE) {
			*match = false;
			dom_node_unref(n);
			break;
		}

		err = dom_node_get_next_sibling(n, &next);
		if (err != DOM_NO_ERR) {
			dom_node_unref(n);
			return CSS_BADPARM;
		}
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
	if (property == CSS_PROP_COLOR) {
		hint->data.color = 0xff000000;
		hint->status = CSS_COLOR_COLOR;
	} else if (property == CSS_PROP_FONT_FAMILY) {
		hint->data.strings = NULL;
		switch (nsoption_int(font_default)) {
		case PLOT_FONT_FAMILY_SANS_SERIF:
			hint->status = CSS_FONT_FAMILY_SANS_SERIF;
			break;
		case PLOT_FONT_FAMILY_SERIF:
			hint->status = CSS_FONT_FAMILY_SERIF;
			break;
		case PLOT_FONT_FAMILY_MONOSPACE:
			hint->status = CSS_FONT_FAMILY_MONOSPACE;
			break;
		case PLOT_FONT_FAMILY_CURSIVE:
			hint->status = CSS_FONT_FAMILY_CURSIVE;
			break;
		case PLOT_FONT_FAMILY_FANTASY:
			hint->status = CSS_FONT_FAMILY_FANTASY;
			break;
		}
	} else if (property == CSS_PROP_QUOTES) {
		/** \todo Not exactly useful :) */
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

css_error set_libcss_node_data(void *pw, void *node, void *libcss_node_data)
{
	dom_node *n = node;
	dom_exception err;
	void *old_node_data;

	/* Set this node's node data */
	err = dom_node_set_user_data(n,
			corestring_dom___ns_key_libcss_node_data,
			libcss_node_data, nscss_dom_user_data_handler,
			(void *) &old_node_data);
	if (err != DOM_NO_ERR) {
		return CSS_NOMEM;
	}

	assert(old_node_data == NULL);

	return CSS_OK;
}

css_error get_libcss_node_data(void *pw, void *node, void **libcss_node_data)
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
