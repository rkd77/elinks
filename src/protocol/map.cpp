#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "protocol/map.h"

#include "elinks.h"

#include <map>
#include <string>

static std::map<std::string, std::string> uri_pos_map;

void
save_in_uri_map(char *url, char *pos)
{
	uri_pos_map[url] = pos;
}

const char *
get_url_pos(char *url)
{
	auto search = uri_pos_map.find(url);

	if (search == uri_pos_map.end()) {
		return NULL;
	}
	return (search->second).c_str();
}

void
del_position(char *url)
{
	uri_pos_map.erase(url);
}
