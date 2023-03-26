/* map temporary file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstddef>
#include <map>
#include "ecmascript/libdom/quickjs/mapa.h"
#include "ecmascript/quickjs.h"

void
attr_save_in_map(void *m, void *node, JSValueConst value)
{
	std::map<void *, JSValueConst> *mapa = static_cast<std::map<void *, JSValueConst> *>(m);
	(*mapa)[node] = value;
}

void *
attr_create_new_attrs_map(void)
{
	std::map<void *, JSValueConst> *mapa = new std::map<void *, JSValueConst>;

	return (void *)mapa;
}

void *
attr_create_new_attributes_map(void)
{
	std::map<void *, JSValueConst> *mapa = new std::map<void *, JSValueConst>;

	return (void *)mapa;
}

void *
attr_create_new_attributes_map_rev(void)
{
	std::map<JSValueConst, void *> *mapa = new std::map<JSValueConst, void *>;

	return (void *)mapa;
}

void *
attr_create_new_collections_map(void)
{
	std::map<void *, JSValueConst> *mapa = new std::map<void *, JSValueConst>;

	return (void *)mapa;
}

void *
attr_create_new_collections_map_rev(void)
{
	std::map<JSValueConst, void *> *mapa = new std::map<JSValueConst, void *>;

	return (void *)mapa;
}

void
attr_clear_map(void *m)
{
	std::map<void *, JSValueConst> *mapa = static_cast<std::map<void *, JSValueConst> *>(m);
	mapa->clear();
}

JSValue
attr_find_in_map(void *m, void *node)
{
	std::map<void *, JSValueConst> *mapa = static_cast<std::map<void *, JSValueConst> *>(m);

	if (!mapa) {
		return JS_NULL;
	}
	auto value = (*mapa).find(node);

	if (value == (*mapa).end()) {
		return JS_NULL;
	}
	return value->second;
}

void
attr_erase_from_map(void *m, void *node)
{
	std::map<void *, JSValueConst> *mapa = static_cast<std::map<void *, JSValueConst> *>(m);
	mapa->erase(node);
}

void
attr_save_in_map_rev(void *m, JSValueConst value, void *node)
{
	std::map<JSValueConst, void *> *mapa = static_cast<std::map<JSValueConst, void *> *>(m);
	(*mapa)[value] = node;
}

void
attr_clear_map_rev(void *m)
{
	std::map<JSValueConst, void *> *mapa = static_cast<std::map<JSValueConst, void *> *>(m);
	mapa->clear();
}

void *
attr_find_in_map_rev(void *m, JSValueConst value)
{
	std::map<JSValueConst, void *> *mapa = static_cast<std::map<JSValueConst, void *> *>(m);

	if (!mapa) {
		return NULL;
	}
	auto v = (*mapa).find(value);

	if (v == (*mapa).end()) {
		return NULL;
	}
	return v->second;
}

void attr_erase_from_map_rev(void *m, JSValueConst value)
{
	std::map<JSValueConst, void *> *mapa = static_cast<std::map<JSValueConst, void *> *>(m);
	mapa->erase(value);
}
