#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "protocol/map.h"
#include "util/string.h"

#include "elinks.h"

#include <list>
#include <map>
#include <string>

static std::map<std::string, std::list<std::string>> uri_pos_map;

void
save_in_uri_map(char *url, char *pos)
{
	uri_pos_map[url].push_back(pos);
}

char *
get_url_pos(char *url)
{
	auto it = uri_pos_map.find(url);

	if (it == uri_pos_map.end() || it->second.empty()) {
		return NULL;
	}
	std::string value = it->second.front();
	it->second.pop_front();

	if (it->second.empty()) {
		uri_pos_map.erase(it);
	}

	return stracpy(value.c_str());
}
