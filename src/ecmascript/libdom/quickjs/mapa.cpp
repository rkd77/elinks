/* map temporary file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstddef>
#include <map>
#include "ecmascript/libdom/quickjs/mapa.h"

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
