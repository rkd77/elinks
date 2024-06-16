/* The MuJS DOMRect implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "ecmascript/ecmascript.h"
#include "ecmascript/libdom/dom.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/domrect.h"

struct eljs_domrect {
	double x;
	double y;
	double width;
	double height;
	double top;
	double right;
	double bottom;
	double left;
};

static void mjs_domRect_get_property_bottom(js_State *J);
static void mjs_domRect_get_property_height(js_State *J);
static void mjs_domRect_get_property_left(js_State *J);
static void mjs_domRect_get_property_right(js_State *J);
static void mjs_domRect_get_property_top(js_State *J);
static void mjs_domRect_get_property_width(js_State *J);
static void mjs_domRect_get_property_x(js_State *J);
static void mjs_domRect_get_property_y(js_State *J);

static void mjs_domRect_set_property_bottom(js_State *J);
static void mjs_domRect_set_property_height(js_State *J);
static void mjs_domRect_set_property_left(js_State *J);
static void mjs_domRect_set_property_right(js_State *J);
static void mjs_domRect_set_property_top(js_State *J);
static void mjs_domRect_set_property_width(js_State *J);
static void mjs_domRect_set_property_x(js_State *J);
static void mjs_domRect_set_property_y(js_State *J);

static void
mjs_domRect_finalizer(js_State *J, void *val)
{
	struct eljs_domrect *d = (struct eljs_domrect *)val;

	if (d) {
		mem_free(d);
	}
}

void
mjs_push_domRect(js_State *J)
{
	struct eljs_domrect *d = (struct eljs_domrect *)mem_calloc(1, sizeof(*d));

	if (!d) {
		js_error(J, "out of memory");
		return;
	}

	js_newobject(J);
	{
		js_newuserdata(J, "DOMRect", d, mjs_domRect_finalizer);
		addproperty(J, "bottom", mjs_domRect_get_property_bottom, mjs_domRect_set_property_bottom);
		addproperty(J, "height", mjs_domRect_get_property_height, mjs_domRect_set_property_height);
		addproperty(J, "left", mjs_domRect_get_property_left, mjs_domRect_set_property_left);
		addproperty(J, "right", mjs_domRect_get_property_right, mjs_domRect_set_property_right);
		addproperty(J, "top", mjs_domRect_get_property_top, mjs_domRect_set_property_top);
		addproperty(J, "width", mjs_domRect_get_property_width, mjs_domRect_set_property_width);
		addproperty(J, "x", mjs_domRect_get_property_x, mjs_domRect_set_property_x);
		addproperty(J, "y", mjs_domRect_get_property_y, mjs_domRect_set_property_y);
	}
}

static void
mjs_domRect_get_property_bottom(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, d->bottom);
}

static void
mjs_domRect_get_property_height(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, d->height);
}

static void
mjs_domRect_get_property_left(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, d->left);
}

static void
mjs_domRect_get_property_right(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, d->right);
}

static void
mjs_domRect_get_property_top(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, d->top);
}

static void
mjs_domRect_get_property_width(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, d->width);
}

static void
mjs_domRect_get_property_x(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, d->x);
}

static void
mjs_domRect_get_property_y(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	js_pushnumber(J, d->y);
}

static void
mjs_domRect_set_property_bottom(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	d->bottom = js_tonumber(J, 1);
	js_pushundefined(J);
}

static void
mjs_domRect_set_property_height(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	d->height = js_tonumber(J, 1);
	js_pushundefined(J);
}

static void
mjs_domRect_set_property_left(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	d->left = js_tonumber(J, 1);
	js_pushundefined(J);
}

static void
mjs_domRect_set_property_right(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	d->right = js_tonumber(J, 1);
	js_pushundefined(J);
}

static void
mjs_domRect_set_property_top(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	d->top = js_tonumber(J, 1);
	js_pushundefined(J);
}

static void
mjs_domRect_set_property_width(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	d->width = js_tonumber(J, 1);
	js_pushundefined(J);
}

static void
mjs_domRect_set_property_x(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	d->x = js_tonumber(J, 1);
	js_pushundefined(J);
}

static void
mjs_domRect_set_property_y(js_State *J)
{
#ifdef ECMASCRIPT_DEBUG
	fprintf(stderr, "%s:%s\n", __FILE__, __FUNCTION__);
#endif
	struct eljs_domrect *d = (struct eljs_domrect *)js_touserdata(J, 0, "DOMRect");

	if (!d) {
		js_pushnull(J);
		return;
	}
	d->y = js_tonumber(J, 1);
	js_pushundefined(J);
}
