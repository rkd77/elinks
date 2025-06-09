#include "terminal/map.h"
#include <map>

#include <stdio.h>

static std::map<unsigned int, unsigned int> map_ID;
static std::map<unsigned int, unsigned int> reverse_map_ID;

// -1 not found
int
get_id_from_ID(unsigned int ID)
{
	auto search = map_ID.find(ID);

	if (search == map_ID.end()) {
		return -1;
	}

	return search->second;

}

void
set_id_ID(unsigned int id, unsigned int ID)
{
	map_ID[ID] = id;
	reverse_map_ID[id] = ID;
}

void
remove_id(unsigned int id)
{
	auto search = reverse_map_ID.find(id);

	if (search == reverse_map_ID.end()) {
		return;
	}
	unsigned int ID = search->second;
	reverse_map_ID.erase(search);

	auto search2 = map_ID.find(ID);

	if (search2 != map_ID.end()) {
		map_ID.erase(search2);
	}
}
