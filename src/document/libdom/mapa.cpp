/* map temporary file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <cstddef>
#include <map>
#include "document/libdom/mapa.h"

void
save_in_map(void *m, void *node, int length)
{
	std::map<int, void *> *mapa = static_cast<std::map<int, void *> *>(m);
	(*mapa)[length] = node;
}

void *
create_new_element_map(void)
{
	std::map<int, void *> *mapa = new std::map<int, void *>;

	return (void *)mapa;
}

void
clear_map(void *m)
{
	std::map<int, void *> *mapa = static_cast<std::map<int, void *> *>(m);
	mapa->clear();
}

void *
find_in_map(void *m, int offset)
{
	std::map<int, void *> *mapa = static_cast<std::map<int, void *> *>(m);

	if (!mapa) {
		return NULL;
	}
	auto element = (*mapa).find(offset);

	if (element == (*mapa).end()) {
		return NULL;
	}
	return (void *)element->second;
}
