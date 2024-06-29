/* The MuJS dataset object implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef CONFIG_LIBDOM
#include <dom/dom.h>
#include <dom/bindings/hubbub/parser.h>
#endif

#include "elinks.h"

#include "document/libdom/corestrings.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/ecmascript-c.h"
#include "ecmascript/mujs/mapa.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/dataset.h"
#include "ecmascript/mujs/element.h"

static void
mjs_dataset_finalizer(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node *el = (dom_node *)(node);

	if (el) {
		dom_node_unref(el);
	}
}

static int
mjs_obj_dataset_has(js_State *J, void *p, const char *property)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	if (!property) {
		return 0;
	}
	dom_node *el = (dom_node *)p;
	struct string data;

	if (!el ||!init_string(&data)) {
		return 0;
	}
	camel_to_html(&data, property);
	dom_string *attr_name = NULL;
	dom_exception exc = dom_string_create(data.source, data.length, &attr_name);
	done_string(&data);

	if (exc != DOM_NO_ERR || !attr_name) {
		return 0;
	}
	dom_string *attr_value = NULL;
	exc = dom_element_get_attribute(el, attr_name, &attr_value);
	dom_string_unref(attr_name);

	if (exc != DOM_NO_ERR || !attr_value) {
		return 0;
	}
	js_pushstring(J, dom_string_data(attr_value));
	dom_string_unref(attr_value);

	return 1;
}

static int
mjs_obj_dataset_put(js_State *J, void *p, const char *property)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	if (!property) {
		return 0;
	}
	const char *value = js_tostring(J, -1);

	if (!value) {
		return 0;
	}
	dom_node *el = (dom_node *)p;
	struct string data;

	if (!el ||!init_string(&data)) {
		return 0;
	}
	camel_to_html(&data, property);
	dom_string *attr_name = NULL;
	dom_exception exc = dom_string_create(data.source, data.length, &attr_name);
	done_string(&data);

	if (exc != DOM_NO_ERR || !attr_name) {
		return 0;
	}
	dom_string *attr_value = NULL;
	exc = dom_string_create(value, strlen(value), &attr_value);

	if (exc != DOM_NO_ERR || !attr_value) {
		dom_string_unref(attr_name);
		return 0;
	}
	exc = dom_element_set_attribute(el, attr_name, attr_value);
	dom_string_unref(attr_name);
	dom_string_unref(attr_value);
	interpreter->changed = true;

	return 1;
}

static int
mjs_obj_dataset_del(js_State *J, void *p, const char *property)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct ecmascript_interpreter *interpreter = (struct ecmascript_interpreter *)js_getcontext(J);

	if (!property) {
		return 0;
	}
	dom_node *el = (dom_node *)p;
	struct string data;

	if (!el ||!init_string(&data)) {
		return 0;
	}
	camel_to_html(&data, property);
	dom_string *attr_name = NULL;
	dom_exception exc = dom_string_create(data.source, data.length, &attr_name);
	done_string(&data);

	if (exc != DOM_NO_ERR || !attr_name) {
		return 0;
	}
	dom_string *attr_value = NULL;
	exc = dom_element_remove_attribute(el, attr_name);
	dom_string_unref(attr_name);
	interpreter->changed = true;

	return 1;
}

void
mjs_push_dataset(js_State *J, void *node)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	dom_node_ref(node);

	js_newobject(J);
	{
		js_newuserdatax(J, "dataset", node, mjs_obj_dataset_has, mjs_obj_dataset_put, mjs_obj_dataset_del, mjs_dataset_finalizer);
	}
}
