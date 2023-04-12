/* map temporary file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstddef>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "ecmascript/libdom/mujs/mapa.h"
#include "ecmascript/mujs.h"
#include "ecmascript/mujs/xhr.h"
#include "util/memory.h"
#include "util/string.h"

void
attr_save_in_map(void *m, void *node, void *value)
{
	std::map<void *, void *> *mapa = static_cast<std::map<void *, void *> *>(m);
	(*mapa)[node] = value;
}

void *
attr_create_new_attrs_map(void)
{
	std::map<void *, void *> *mapa = new std::map<void *, void *>;

	return (void *)mapa;
}

struct classcomp {
	bool operator() (const std::string& lhs, const std::string& rhs) const
	{
		return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
	}
};

void
attr_clear_map(void *m)
{
	std::map<void *, void *> *mapa = static_cast<std::map<void *, void *> *>(m);
	mapa->clear();
}

void
delete_map_str(void *m)
{
	std::map<std::string, std::string> *mapa = static_cast<std::map<std::string, std::string> *>(m);

	if (mapa) {
		delete(mapa);
	}
}

void
attr_delete_map(void *m)
{
	std::map<void *, void *> *mapa = static_cast<std::map<void *, void *> *>(m);

	if (mapa) {
		delete(mapa);
	}
}

void *
attr_find_in_map(void *m, void *node)
{
	std::map<void *, void *> *mapa = static_cast<std::map<void *, void *> *>(m);

	if (!mapa) {
		return NULL;
	}
	auto value = (*mapa).find(node);

	if (value == (*mapa).end()) {
		return NULL;
	}
	return value->second;
}

void
attr_erase_from_map(void *m, void *node)
{
	std::map<void *, void *> *mapa = static_cast<std::map<void *, void *> *>(m);
	mapa->erase(node);
}
