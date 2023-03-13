/* CSS */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include <stdio.h>

#undef restrict
#define restrict __restrict
#include <libcss/libcss.h>

#include "cache/cache.h"
#include "document/css2/css.h"
#include "document/html/internal.h"
#include "util/string.h"

#include <iostream>
#include <libxml++/libxml++.h>
#include <map>
#include <string>
#include <sstream>

/* This macro is used to silence compiler warnings about unused function
 * arguments. */
#define UNUSED(x) ((x) = (x))

#ifndef CONFIG_LIBDOM
/* Function declarations. */
css_error resolve_url(void *pw,
		const char *base, lwc_string *rel, lwc_string **abs);

static css_error node_name(void *pw, void *node,
		css_qname *qname);
static css_error node_classes(void *pw, void *node,
		lwc_string ***classes, uint32_t *n_classes);
static css_error node_id(void *pw, void *node,
		lwc_string **id);
static css_error named_ancestor_node(void *pw, void *node,
		const css_qname *qname,
		void **ancestor);
static css_error named_parent_node(void *pw, void *node,
		const css_qname *qname,
		void **parent);
static css_error named_sibling_node(void *pw, void *node,
		const css_qname *qname,
		void **sibling);
static css_error named_generic_sibling_node(void *pw, void *node,
		const css_qname *qname,
		void **sibling);
static css_error parent_node(void *pw, void *node, void **parent);
static css_error sibling_node(void *pw, void *node, void **sibling);
static css_error node_has_name(void *pw, void *node,
		const css_qname *qname,
		bool *match);
static css_error node_has_class(void *pw, void *node,
		lwc_string *name,
		bool *match);
static css_error node_has_id(void *pw, void *node,
		lwc_string *name,
		bool *match);
static css_error node_has_attribute(void *pw, void *node,
		const css_qname *qname,
		bool *match);
static css_error node_has_attribute_equal(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_dashmatch(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_includes(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_prefix(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_suffix(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_has_attribute_substring(void *pw, void *node,
		const css_qname *qname,
		lwc_string *value,
		bool *match);
static css_error node_is_root(void *pw, void *node, bool *match);
static css_error node_count_siblings(void *pw, void *node,
		bool same_name, bool after, int32_t *count);
static css_error node_is_empty(void *pw, void *node, bool *match);
static css_error node_is_link(void *pw, void *node, bool *match);
static css_error node_is_visited(void *pw, void *node, bool *match);
static css_error node_is_hover(void *pw, void *node, bool *match);
static css_error node_is_active(void *pw, void *node, bool *match);
static css_error node_is_focus(void *pw, void *node, bool *match);
static css_error node_is_enabled(void *pw, void *node, bool *match);
static css_error node_is_disabled(void *pw, void *node, bool *match);
static css_error node_is_checked(void *pw, void *node, bool *match);
static css_error node_is_target(void *pw, void *node, bool *match);
static css_error node_is_lang(void *pw, void *node,
		lwc_string *lang, bool *match);
static css_error node_presentational_hint(void *pw, void *node,
		uint32_t *nhints, css_hint **hints);
static css_error ua_default_for_property(void *pw, uint32_t property,
		css_hint *hint);
static css_error compute_font_size(void *pw, const css_hint *parent,
		css_hint *size);
static css_error set_libcss_node_data(void *pw, void *n,
		void *libcss_node_data);
static css_error get_libcss_node_data(void *pw, void *n,
		void **libcss_node_data);

/* Table of function pointers for the LibCSS Select API. */
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
	compute_font_size,
	set_libcss_node_data,
	get_libcss_node_data
};

css_error
resolve_url(void *pw, const char *base, lwc_string *rel, lwc_string **abs)
{
//	fprintf(stderr, "resolve_url: base=%s\n", base);
//	fprintf(stderr, "rel=%s\n", lwc_string_data(rel));
	lwc_error lerror;

	char *url = straconcat(base, lwc_string_data(rel), NULL);

	if (!url) {
		*abs = NULL;
		return CSS_NOMEM;
	}

	lerror = lwc_intern_string(url, strlen(url), abs);
	if (lerror != lwc_error_ok) {
		*abs = NULL;
		return lerror == lwc_error_oom ? CSS_NOMEM : CSS_INVALID;
	}
//	fprintf(stderr, "abs=%s\n", lwc_string_data(*abs));

	return CSS_OK;
}

/* Select handlers. Our "document tree" is actually just a single node, which is
 * a libwapcaplet string containing the element name. Therefore all the
 * functions below except those getting or testing the element name return empty
 * data or false attributes. */
css_error node_name(void *pw, void *n, css_qname *qname)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		return CSS_NOMEM;
	}
	xmlpp::ustring name = element->get_name();
	lwc_intern_string(name.c_str(), name.length(), &qname->name);

	return CSS_OK;
}

css_error node_classes(void *pw, void *n,
		lwc_string ***classes, uint32_t *n_classes)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	lwc_string **klases = NULL;
	*classes = NULL;
	*n_classes = 0;

	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		return CSS_OK;
	}
	std::string cl = element->get_attribute_value("class");

	std::stringstream stream(cl);
	std::string word;

	uint32_t counter = 0;
	while (stream >> word) {
		counter++;
	}

	if (!counter) {
		return CSS_OK;
	}
	klases = (lwc_string **)malloc(counter * sizeof(lwc_string *));

//fprintf(stderr, "counter=%d\n", counter);
//fprintf(stderr, "classes=%p\n", classes);

	if (!klases) {
		return CSS_OK;
	}

	*n_classes = counter;
	counter = 0;
	std::stringstream stream2(cl);

	while (stream2 >> word) {
//fprintf(stderr, "word=%s\n", word.c_str());
		lwc_intern_string(word.c_str(), word.length(), &klases[counter++]);
	}
	*classes = klases;

	return CSS_OK;
}

css_error node_id(void *pw, void *n, lwc_string **id)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		*id = NULL;
		return CSS_OK;
	}
	xmlpp::ustring v = element->get_attribute_value("id");
	lwc_intern_string(v.c_str(), v.length(), id);

	return CSS_OK;
}

css_error named_ancestor_node(void *pw, void *n,
		const css_qname *qname,
		void **ancestor)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	*ancestor = NULL;
	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		return CSS_OK;
	}

	xmlpp::Element *node;
	size_t vlen = lwc_string_length(qname->name);
	const char *text = lwc_string_data(qname->name);

	for (node = element->get_parent(); node != NULL; node = node->get_parent()) {
		xmlpp::ustring name = node->get_name();

		if (name.length() == vlen && !strncasecmp(text, name.c_str(), vlen)) {
			*ancestor = (void *)node;
			break;
		}
	}

	return CSS_OK;
}

css_error named_parent_node(void *pw, void *n,
		const css_qname *qname,
		void **parent)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	*parent = NULL;
	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		return CSS_OK;
	}

	xmlpp::Element *node;
	size_t vlen = lwc_string_length(qname->name);
	const char *text = lwc_string_data(qname->name);

	for (node = element->get_parent(); node != NULL; node = node->get_parent()) {
		xmlpp::ustring name = node->get_name();

		if (name.length() == vlen && !strncasecmp(text, name.c_str(), vlen)) {
			*parent = (void *)node;
			break;
		}
	}

	return CSS_OK;
}

css_error named_generic_sibling_node(void *pw, void *n,
		const css_qname *qname,
		void **sibling)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	*sibling = NULL;

	xmlpp::Node *node = static_cast<xmlpp::Node *>(n);

	if (!node) {
		return CSS_OK;
	}

	node = node->get_previous_sibling();
	size_t vlen = lwc_string_length(qname->name);
	const char *text = lwc_string_data(qname->name);

	while (node) {
		if (dynamic_cast<xmlpp::Element *>(node)) {
			xmlpp::ustring name = node->get_name();

			if (vlen == name.length() && !strncasecmp(text, name.c_str(), vlen)) {
				*sibling = (void *)node;
				break;
			}
		}
		node = node->get_previous_sibling();
	}

	return CSS_OK;
}

css_error named_sibling_node(void *pw, void *n,
		const css_qname *qname,
		void **sibling)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	*sibling = NULL;
	xmlpp::Node *node = static_cast<xmlpp::Node *>(n);

	if (!node) {
		return CSS_OK;
	}

	node = node->get_previous_sibling();
	while (node) {
		if (dynamic_cast<xmlpp::Element *>(node)) {
			break;
		}
		node = node->get_previous_sibling();
	}

	if (node) {
		xmlpp::ustring name = node->get_name();
		size_t vlen = lwc_string_length(qname->name);

		if (vlen == name.length() && !strncasecmp(lwc_string_data(qname->name), name.c_str(), vlen)) {
			*sibling = (void *)node;
		}
	}

	return CSS_OK;
}

css_error parent_node(void *pw, void *n, void **parent)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		*parent = NULL;
		return CSS_OK;
	}

	xmlpp::Element *parent_element = element->get_parent();
	*parent = (void *)parent_element;

	return CSS_OK;
}

css_error sibling_node(void *pw, void *n, void **sibling)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	*sibling = NULL;
	xmlpp::Node *node = static_cast<xmlpp::Node *>(n);

	if (!node) {
		return CSS_OK;
	}
	node = node->get_previous_sibling();

	while (node) {
		if (dynamic_cast<xmlpp::Element *>(node)) {
			*sibling = (void *)node;
			break;
		}
		node = node->get_previous_sibling();
	}

	return CSS_OK;
}

css_error node_has_name(void *pw, void *n,
		const css_qname *qname,
		bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		*match = false;
		return CSS_OK;
	}
	xmlpp::ustring node_name = element->get_name();
	*match = (node_name.length() == lwc_string_length(qname->name) && !strncasecmp(node_name.c_str(), lwc_string_data(qname->name), node_name.length()));

	return CSS_OK;
}

css_error node_has_class(void *pw, void *n,
		lwc_string *name,
		bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		*match = false;
		return CSS_OK;
	}
	xmlpp::ustring cl = element->get_attribute_value("class");

	std::stringstream stream(cl);
	std::string word;

	while (stream >> word) {
		if (word.length() != lwc_string_length(name)) {
			continue;
		}
		if (!memcmp(word.c_str(), lwc_string_data(name), word.length())) {
			*match = true;
			return CSS_OK;
		}
	}
	*match = false;
	return CSS_OK;
}

css_error node_has_id(void *pw, void *n,
		lwc_string *name,
		bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		*match = false;
		return CSS_OK;
	}
	xmlpp::ustring id = element->get_attribute_value("id");
	*match = (id.length() == lwc_string_length(name) && !memcmp(id.c_str(), lwc_string_data(name), id.length()));

	return CSS_OK;
}

css_error node_has_attribute(void *pw, void *n,
		const css_qname *qname,
		bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		*match = false;
		return CSS_OK;
	}

	xmlpp::ustring attr = element->get_attribute_value(lwc_string_data(qname->name));
	*match = attr.length() > 0;

	return CSS_OK;
}

css_error node_has_attribute_equal(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		*match = false;
		return CSS_OK;
	}

	xmlpp::ustring attr = element->get_attribute_value(lwc_string_data(qname->name));
	*match = (attr.length() > 0 && attr.length() == lwc_string_length(value) && !strncasecmp(attr.c_str(), lwc_string_data(value), attr.length()));

	return CSS_OK;
}

css_error node_has_attribute_dashmatch(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);

	if (!element) {
		*match = false;
		return CSS_OK;
	}
	size_t vlen = lwc_string_length(value);

	if (!vlen) {
		*match = false;
		return CSS_OK;
	}
	xmlpp::ustring attr = element->get_attribute_value(lwc_string_data(qname->name));
	*match = (attr.length() > 0 && attr.length() == vlen && !strncasecmp(attr.c_str(), lwc_string_data(value), vlen));

	if (*match) {
		return CSS_OK;
	}
	*match = (attr.length() > vlen && '-' == attr[vlen] && !strncasecmp(attr.c_str(), lwc_string_data(value), vlen));

	return CSS_OK;
}

css_error node_has_attribute_includes(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);
	*match = false;

	if (!element) {
		return CSS_OK;
	}
	size_t vlen = lwc_string_length(value);

	if (!vlen) {
		return CSS_OK;
	}
	xmlpp::ustring attr = element->get_attribute_value(lwc_string_data(qname->name));

	if (attr.length() == 0) {
		return CSS_OK;
	}

	const char *p;
	const char *start = (const char *)attr.c_str();
	const char *end = start + attr.length();

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
	return CSS_OK;
}

css_error node_has_attribute_prefix(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);
	*match = false;

	if (!element) {
		return CSS_OK;
	}
	size_t vlen = lwc_string_length(value);

	if (!vlen) {
		return CSS_OK;
	}
	xmlpp::ustring attr = element->get_attribute_value(lwc_string_data(qname->name));

	if (attr.length() == 0) {
		return CSS_OK;
	}
	*match = (attr.length() >= vlen && !strncasecmp(attr.c_str(), lwc_string_data(value), vlen));

	return CSS_OK;
}

css_error node_has_attribute_suffix(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);
	*match = false;

	if (!element) {
		return CSS_OK;
	}
	size_t vlen = lwc_string_length(value);

	if (!vlen) {
		return CSS_OK;
	}
	xmlpp::ustring attr = element->get_attribute_value(lwc_string_data(qname->name));

	if (attr.length() == 0) {
		return CSS_OK;
	}
	const char *start = attr.c_str() + attr.length() - vlen;
	*match = (attr.length() >= vlen && !strncasecmp(start, lwc_string_data(value), vlen));

	return CSS_OK;
}

css_error node_has_attribute_substring(void *pw, void *n,
		const css_qname *qname,
		lwc_string *value,
		bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);
	*match = false;

	if (!element) {
		return CSS_OK;
	}
	size_t vlen = lwc_string_length(value);

	if (!vlen) {
		return CSS_OK;
	}
	xmlpp::ustring attr = element->get_attribute_value(lwc_string_data(qname->name));

	if (attr.length() == 0) {
		return CSS_OK;
	}

	const char *vdata = lwc_string_data(value);
	const char *start = (const char *)attr.c_str();
	size_t len = attr.length();
	const char *last_start = start + len - vlen;

	if (len >= vlen) {
		while (start <= last_start) {
			if (strncasecmp(start, vdata, vlen) == 0) {
				*match = true;
				break;
			}

			start++;
		}
	}
	return CSS_OK;
}

css_error node_is_first_child(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);
	*match = false;

	if (!element) {
		return CSS_OK;
	}
	xmlpp::Element *parent = element->get_parent();

	if (!parent) {
		return CSS_OK;
	}
	*match = parent->get_first_child() == element;

	return CSS_OK;
}

css_error node_is_root(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	xmlpp::Element *element = static_cast<xmlpp::Element *>(n);
	*match = false;

	if (!element) {
		return CSS_OK;
	}
	xmlpp::Element *parent = element->get_parent();
	*match = (parent == NULL);

	return CSS_OK;
}

static int
node_count_siblings_check(xmlpp::Node *node, bool check_name, xmlpp::ustring name)
{
	int ret = 0;

	if (node == NULL) {
		return 0;
	}

	if (dynamic_cast<xmlpp::Element *>(node) == NULL) {
		return 0;
	}

	if (check_name) {
		xmlpp::ustring node_name = node->get_name();

		if (name.length() == node_name.length() && !strncasecmp(node_name.c_str(), name.c_str(), name.length())) {
			ret = 1;
		}
	} else {
		ret = 1;
	}

	return ret;
}


css_error node_count_siblings(void *pw, void *n,
		bool same_name, bool after, int32_t *count)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	int32_t cnt = 0;
	xmlpp::Node *node = static_cast<xmlpp::Node *>(n);

	if (!node) {
		return CSS_NOMEM;
	}
	xmlpp::ustring node_name;

	if (same_name) {
		node_name = node->get_name();
	}

	if (after) {
		do {
			node = node->get_next_sibling();
			cnt += node_count_siblings_check(node, same_name, node_name);
		} while (node);
	} else {
		do {
			node = node->get_previous_sibling();
			cnt += node_count_siblings_check(node, same_name, node_name);
		} while (node);
	}
	*count = cnt;

	return CSS_OK;
}

css_error node_is_empty(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	*match = true;
	xmlpp::Node *node = static_cast<xmlpp::Node *>(n);

	if (!node) {
		return CSS_OK;
	}
	node = node->get_first_child();

	while (node) {
		if (dynamic_cast<xmlpp::Element *>(node) || dynamic_cast<xmlpp::TextNode *>(node)) {
			*match = false;
			break;
		}
		node = node->get_next_sibling();
	}

	return CSS_OK;
}

css_error node_is_link(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	*match = false;

	xmlpp::Element *node = static_cast<xmlpp::Element *>(n);

	if (!node) {
		return CSS_OK;
	}
	if (node->get_name() != "a" && node->get_name() != "A") {
		return CSS_OK;
	}

	xmlpp::ustring href = node->get_attribute_value("href");
	*match = href.length() > 0;

	return CSS_OK;
}

css_error node_is_visited(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_hover(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_active(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_focus(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_enabled(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_disabled(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_checked(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}

css_error node_is_target(void *pw, void *n, bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	UNUSED(pw);
	UNUSED(n);
	*match = false;
	return CSS_OK;
}


css_error node_is_lang(void *pw, void *n,
		lwc_string *lang,
		bool *match)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	UNUSED(pw);
	UNUSED(n);
	UNUSED(lang);
	*match = false;
	return CSS_OK;
}

css_error node_presentational_hint(void *pw, void *node,
		uint32_t *nhints, css_hint **hints)
{
//fprintf(stderr, "%s: node=%s\n", __FUNCTION__, node);
	UNUSED(pw);
	UNUSED(node);
	*nhints = 0;
	*hints = NULL;
	return CSS_OK;
}

css_error ua_default_for_property(void *pw, uint32_t property, css_hint *hint)
{
	UNUSED(pw);

	if (property == CSS_PROP_COLOR) {
		hint->data.color = 0x00000000;
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

css_error compute_font_size(void *pw, const css_hint *parent, css_hint *size)
{
	static css_hint_length sizes[] = {
		{ FLTTOFIX(6.75), CSS_UNIT_PT },
		{ FLTTOFIX(7.50), CSS_UNIT_PT },
		{ FLTTOFIX(9.75), CSS_UNIT_PT },
		{ FLTTOFIX(12.0), CSS_UNIT_PT },
		{ FLTTOFIX(13.5), CSS_UNIT_PT },
		{ FLTTOFIX(18.0), CSS_UNIT_PT },
		{ FLTTOFIX(24.0), CSS_UNIT_PT }
	};
	const css_hint_length *parent_size;

	UNUSED(pw);

	/* Grab parent size, defaulting to medium if none */
	if (parent == NULL) {
		parent_size = &sizes[CSS_FONT_SIZE_MEDIUM - 1];
	} else {
		assert(parent->status == CSS_FONT_SIZE_DIMENSION);
		assert(parent->data.length.unit != CSS_UNIT_EM);
		assert(parent->data.length.unit != CSS_UNIT_EX);
		parent_size = &parent->data.length;
	}

	assert(size->status != CSS_FONT_SIZE_INHERIT);

	if (size->status < CSS_FONT_SIZE_LARGER) {
		/* Keyword -- simple */
		size->data.length = sizes[size->status - 1];
	} else if (size->status == CSS_FONT_SIZE_LARGER) {
		/** \todo Step within table, if appropriate */
		size->data.length.value =
				FMUL(parent_size->value, FLTTOFIX(1.2));
		size->data.length.unit = parent_size->unit;
	} else if (size->status == CSS_FONT_SIZE_SMALLER) {
		/** \todo Step within table, if appropriate */
		size->data.length.value =
				FMUL(parent_size->value, FLTTOFIX(1.2));
		size->data.length.unit = parent_size->unit;
	} else if (size->data.length.unit == CSS_UNIT_EM ||
			size->data.length.unit == CSS_UNIT_EX) {
		size->data.length.value =
			FMUL(size->data.length.value, parent_size->value);

		if (size->data.length.unit == CSS_UNIT_EX) {
			size->data.length.value = FMUL(size->data.length.value,
					FLTTOFIX(0.6));
		}

		size->data.length.unit = parent_size->unit;
	} else if (size->data.length.unit == CSS_UNIT_PCT) {
		size->data.length.value = FDIV(FMUL(size->data.length.value,
				parent_size->value), FLTTOFIX(100));
		size->data.length.unit = parent_size->unit;
	}

	size->status = CSS_FONT_SIZE_DIMENSION;

	return CSS_OK;
}

static std::map<void *, void *> node_data;


static css_error
set_libcss_node_data(void *pw, void *n,
		void *libcss_node_data)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);

	node_data[n] = libcss_node_data;

	return CSS_OK;
}

static css_error
get_libcss_node_data(void *pw, void *n,
		void **libcss_node_data)
{
//fprintf(stderr, "%s: n=%s\n", __FUNCTION__, n);
	*libcss_node_data = NULL;

	auto f = node_data.find(n);

	if (f != node_data.end()) {
		*libcss_node_data = f->second;
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
			 struct html_element *element, bool underline, bool bold)
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

css_stylesheet *
create_inline_style(xmlpp::Element *el)
{
	css_stylesheet_params params;
	css_stylesheet *sheet;
	css_error error;

	xmlpp::ustring style = el->get_attribute_value("style");

	if ("" == style) {
		return NULL;
	}

	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_21;
	params.charset = "UTF-8";
	params.url = "";
	params.title = NULL;
	params.allow_quirks = false;
	params.inline_style = true;
	params.resolve = resolve_url;
	params.resolve_pw = NULL;
	params.import = NULL;
	params.import_pw = NULL;
	params.color = NULL;
	params.color_pw = NULL;
	params.font = NULL;
	params.font_pw = NULL;

	error = css_stylesheet_create(&params, &sheet);
	if (error != CSS_OK) {
		fprintf(stderr, "Failed creating sheet: %d", error);
		return NULL;
	}

	error = css_stylesheet_append_data(sheet, style.c_str(), style.length());
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
 * Get style selection results for an element
 *
 * \param ctx             CSS selection context
 * \param n               Element to select for
 * \param media           Permitted media types
 * \param inline_style    Inline style associated with element, or NULL
 * \return Pointer to selection results (containing computed styles),
 *         or NULL on failure
 */
css_select_results *
nscss_get_style(nscss_select_ctx *ctx, xmlpp::Element *n,
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
				compute_font_size, ctx,
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
				compute_font_size, ctx,
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


void
select_css(struct html_context *html_context, struct html_element *html_element)
{
	css_error code;
	uint8_t color_type;
	css_color color_shade;
	css_select_results *style;
	css_stylesheet *inline_style;

	css_media media = {
		.type = CSS_MEDIA_SCREEN,
	};
	int offset = html_element->name - html_context->document->text;
	std::map<int, xmlpp::Element *> *mapa = (std::map<int, xmlpp::Element *> *)html_context->document->element_map;
	if (!mapa) {
		return;
	}
	auto element = (*mapa).find(offset);

	if (element == (*mapa).end()) {
		return;
	}
	xmlpp::Element *el = element->second;

	inline_style = create_inline_style(el);

	nscss_select_ctx ctx = {0};

	/* Populate selection context */
	ctx.ctx = html_context->select_ctx;
	ctx.quirks = false; //(c->quirks == DOM_DOCUMENT_QUIRKS_MODE_FULL);
//	ctx.base_url = c->base_url;
//	ctx.universal = c->universal;
///	ctx.root_style = root_style;
///	ctx.parent_style = parent_style;

	/* Select style for element */
	style = nscss_get_style(&ctx, el, &media, inline_style);

	/* No longer need inline style */
	if (inline_style != NULL) {
		css_stylesheet_destroy(inline_style);
	}

	if (!style) {
		return;
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
	bool bold = is_bold(css_computed_font_weight(style->styles[CSS_PSEUDO_ELEMENT_NONE]));

	apply_font_attribute(html_context, html_element, underline, bold);

	uint8_t font_style = css_computed_font_style(style->styles[CSS_PSEUDO_ELEMENT_NONE]);

	if (font_style) {
		apply_font_style(html_context, html_element, font_style);
	}

	uint8_t list_type = css_computed_list_style_type(style->styles[CSS_PSEUDO_ELEMENT_NONE]);

	if (list_type) {
		apply_list_style(html_context, html_element, list_type);
	}

// Will be addressed later
	bool is_root = (el->get_parent() == NULL) || (el->get_parent()->get_name() == "html");

	uint8_t display = css_computed_display(style->styles[CSS_PSEUDO_ELEMENT_NONE], is_root);

	if (display) {
		apply_display(html_context, html_element, display);
	}

	uint8_t text_align = css_computed_text_align(style->styles[CSS_PSEUDO_ELEMENT_NONE]);

	if (text_align) {
		apply_text_align(html_context, html_element, text_align);
	}

	code = css_select_results_destroy(style);
	if (code != CSS_OK) {
		fprintf(stderr, "css_computed_style_destroy code=%d\n", code);
	}
}

static css_error
handle_import(void *pw, css_stylesheet *parent, lwc_string *url)
{
	struct html_context *html_context = (struct html_context *)pw;
	struct uri *uri = get_uri(lwc_string_data(url), URI_BASE);

	if (!uri) {
		return CSS_NOMEM;
	}

	/* Request the imported stylesheet as part of the document ... */
	html_context->special_f(html_context, SP_STYLESHEET, uri);

	/* ... and then attempt to import from the cache. */
	import_css2(html_context, uri);

	done_uri(uri);

	return CSS_OK;
}

static void
parse_css_common(struct html_context *html_context, const char *text, int length)
{
	css_error code;
	size_t size;
	uint32_t count;
	css_stylesheet_params params;
	css_stylesheet *sheet;

	params.params_version = CSS_STYLESHEET_PARAMS_VERSION_1;
	params.level = CSS_LEVEL_21;
	params.charset = "UTF-8";
	params.url = join_urls(html_context->base_href, "");
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
	if (code != CSS_OK) {
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
	int offset = name - html_context->document->text;
	std::map<int, xmlpp::Element *> *mapa = (std::map<int, xmlpp::Element *> *)html_context->document->element_map;

	if (!mapa) {
		return;
	}
	auto element = (*mapa).find(offset);

	if (element == (*mapa).end()) {
		return;
	}
	xmlpp::Element *el = element->second;

	struct string buf;

	if (!init_string(&buf)) {
		return;
	}

	auto childs = el->get_children();
	auto it = childs.begin();
	auto end = childs.end();

	for (; it != end; ++it) {
		const auto nodeText = dynamic_cast<const xmlpp::ContentNode*>(*it);

		if (nodeText) {
			add_bytes_to_string(&buf, nodeText->get_content().c_str(), nodeText->get_content().length());
		}
	}

	parse_css_common(html_context, buf.source, buf.length);
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
		parse_css_common(html_context, fragment->data, fragment->length);
//		css_parse_stylesheet(css, uri, fragment->data, end);
//		css->import_level--;
	}
}
#endif
