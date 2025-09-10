#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "protocol/map.h"
#include "util/string.h"

#include "elinks.h"

#include <list>
#include <map>
#include <string>

static std::map<std::string, std::list<std::string> *> uri_pos_map;

void
save_in_uri_map(char *url, char *pos)
{
	auto search = uri_pos_map.find(url);

	if (search != uri_pos_map.end()) {
		(search->second)->push_back(pos);
		return;
	}

	std::list<std::string> *first = new std::list<std::string>;
	first->push_back(pos);
	uri_pos_map[url] = first;
}

char *
get_url_pos(char *url)
{
	auto search = uri_pos_map.find(url);

	if (search == uri_pos_map.end()) {
		return NULL;
	}

	if ((search->second)->empty()) {
		uri_pos_map.erase(url);
		return NULL;
	}
	auto first = (search->second)->front();
	char *value = stracpy(first.c_str());
	(search->second)->pop_front();

	return value;
}
