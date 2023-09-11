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

void
save_offset_in_map(void *m, void *node, int offset)
{
	std::map<void *, int> *mapa = static_cast<std::map<void *, int> *>(m);
	(*mapa)[node] = offset;
}

void *
create_new_element_map(void)
{
	std::map<int, void *> *mapa = new std::map<int, void *>;

	return (void *)mapa;
}

void *
create_new_element_map_rev(void)
{
	std::map<void *, int> *mapa = new std::map<void *, int>;

	return (void *)mapa;
}

void
clear_map(void *m)
{
	std::map<int, void *> *mapa = static_cast<std::map<int, void *> *>(m);
	mapa->clear();
}

void
clear_map_rev(void *m)
{
	std::map<void *, int> *mapa = static_cast<std::map<void *, int> *>(m);
	mapa->clear();
}

void
delete_map(void *m)
{
	std::map<int, void *> *mapa = static_cast<std::map<int, void *> *>(m);
	delete mapa;
}

void
delete_map_rev(void *m)
{
	std::map<void *, int> *mapa = static_cast<std::map<void *, int> *>(m);
	delete mapa;
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

int
find_offset(void *m, void *node)
{
	std::map<void *, int> *mapa = static_cast<std::map<void *, int> *>(m);

	if (!mapa) {
		return -1;
	}

	auto element = (*mapa).find(node);

	if (element == (*mapa).end()) {
		return -1;
	}
	return (int)element->second;
}
