/* map temporary file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include <stdbool.h>
#include <stdlib.h>

#include "js/mujs.h"
#include "js/mujs/mapa.h"
#include "js/mujs/xhr.h"
#include "util/hash.h"
#include "util/memory.h"
#include "util/string.h"

void
attr_save_in_map(void *m, void *node, void *value)
{
	struct hash *hash = (struct hash *)m;

	if (hash) {
		char *key = memacpy((const char *)&node, sizeof(node));

		if (key) {
			add_hash_item(hash, key, sizeof(node), value);
		}
	}
}

void *
attr_create_new_map(void)
{
	return (void *)init_hash8();
}

void *
attr_create_new_attrs_map(void)
{
	return (void *)init_hash8();
}

#if 0
struct classcomp {
	bool operator() (const std::string& lhs, const std::string& rhs) const
	{
		return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
	}
};
#endif

void *
attr_create_new_requestHeaders_map(void)
{
	return (void *)init_hash8();
}

void *
attr_create_new_responseHeaders_map(void)
{
	return (void *)init_hash8();
}

void
delete_map_str(void *m)
{
	struct hash *hash = (struct hash *)m;

	if (hash) {
		struct hash_item *item;
		int i;

		foreach_hash_item (item, *hash, i) {
			mem_free_set(&item->key, NULL);
			mem_free_set(&item->value, NULL);
		}
		free_hash(&hash);
	}
}

void
attr_delete_map(void *m)
{
	struct hash *hash = (struct hash *)m;

	if (hash) {
		struct hash_item *item;
		int i;

		foreach_hash_item (item, *hash, i) {
			mem_free_set(&item->key, NULL);
		}
		free_hash(&hash);
	}
}

void *
attr_find_in_map(void *m, void *node)
{
	struct hash *hash = (struct hash *)m;

	if (hash) {
		char *key = memacpy((const char *)&node, sizeof(node));

		if (key) {
			struct hash_item *item = get_hash_item(hash, key, sizeof(node));

			mem_free(key);

			if (item) {
				return item->value;
			}
		}
	}

	return NULL;
}

void
attr_erase_from_map(void *m, void *node)
{
	struct hash *hash = (struct hash *)m;

	if (hash) {
		char *key = memacpy((const char *)&node, sizeof(node));

		if (key) {
			struct hash_item *item = get_hash_item(hash, key, sizeof(node));

			if (item) {
				mem_free_set(&item->key, NULL);
				del_hash_item(hash, item);
			}
			mem_free(key);
		}
	}
}

static int
explode(char *s, const char c, char **header, char **value)
{
	char *next;
	char *colon = strchr(s, c);

	if (!colon) {
		return 0;
	}
	*header = memacpy(s, colon - s);
	next = colon + 1;
	colon = strchr(next, c);

	if (colon) {
		return 1;
	}
	*value = stracpy(next);

	return 2;
}

//static std::map<std::string, std::string> *
static void *
get_requestHeaders(void *h)
{
	return h;
}

//static std::map<std::string, std::string, classcomp> *
static void *
get_responseHeaders(void *h)
{
	return h;
}

void
process_xhr_headers(char *head, struct mjs_xhr *x)
{
	char *next, *next2, *line;
	char *headers = stracpy(head);
	char *space;
	char *statusText;
	char *newline;
	char *stat;
	int status;

	if (!headers) {
		return;
	}
	newline = strchr(headers, '\n');

	if (newline) {
		*newline = '\0';
	}
	space = strchr(headers, ' ');
	if (!space) {
		return;
	}
	stat = space + 1;
	space = strchr(stat, ' ');
	if (space) {
		*space = '\0';
		statusText = space + 1;
	} else {
		statusText = NULL;
	}
	status = atoi(stat);
	struct hash *responseHeaders = get_responseHeaders(x->responseHeaders);
	struct hash_item *item;

	if (newline) {
		next = newline + 1;
		next2 = strchr(next, '\n');
	} else {
		next = next2 = NULL;
	}

	while (1) {
		char *value = NULL;
		char *header = NULL;
		int size;

		if (!next) {
			break;
		}
		line = next;

		if (!next2) {
			next = NULL;
		} else {
			next = next2 + 1;
			next2 = strchr(next, '\n');
		}
		size = explode(line, ':', &header, &value);

		if (size == 0) {
			continue;
		}

		if (size == 1 || !value) {
			mem_free_set(&header, NULL);
			continue;
		}

		if (!header) {
			mem_free(value);
			continue;
		}
		char *normalized_value = normalize(value);
		item = get_hash_item(responseHeaders, header, strlen(header));

		if (item) {
			struct string hh;

			if (init_string(&hh)) {
				add_to_string(&hh, (char *)item->value);
				add_to_string(&hh, ", ");
				add_to_string(&hh, normalized_value);
				mem_free_set(&item->value, hh.source);
			}
		} else {
			add_hash_item(responseHeaders, header, strlen(header), stracpy(normalized_value));
		}
		mem_free(header);
		mem_free(value);
	}
	x->status = status;
	mem_free_set(&x->statusText, null_or_stracpy(statusText));
}

void
set_xhr_header(char *normalized_value, const char *h_name, struct mjs_xhr *x)
{
	struct hash *requestHeaders = get_requestHeaders(x->requestHeaders);
	struct hash_item *item;
	char *key;

	if (!requestHeaders || !h_name) {
		return;
	}
	item = get_hash_item(requestHeaders, h_name, strlen(h_name));

	if (item) {
		struct string hh;

		if (init_string(&hh)) {
			add_to_string(&hh, item->value);
			add_to_string(&hh, ", ");
			add_to_string(&hh, normalized_value);
			mem_free_set(&item->value, hh.source);
		}
		return;
	}
	key = stracpy(h_name);

	if (key) {
		add_hash_item(requestHeaders, key, strlen(key), stracpy(normalized_value));
	}
}

char *
get_output_headers(struct mjs_xhr *x)
{
	struct string output;
	struct hash *hash = (struct hash *)get_responseHeaders(x->responseHeaders);;
	struct hash_item *item;
	int i;

	if (!hash || !init_string(&output)) {
		return NULL;
	}
	foreach_hash_item (item, *hash, i) {
		add_to_string(&output, item->key);
		add_to_string(&output, ": ");
		add_to_string(&output, item->value);
		add_to_string(&output, "\r\n");
	}

	return output.source;
}

char *
get_output_header(const char *header_name, struct mjs_xhr *x)
{
	struct hash *hash = get_responseHeaders(x->responseHeaders);
	struct hash_item *item;

	if (!hash || !header_name) {
		return NULL;
	}
	item = get_hash_item(hash, header_name, strlen(header_name));

	if (!item) {
		return NULL;
	}

	return null_or_stracpy(item->value);
}

const char *good[] = { "background",
	"background-clip",
	"background-color",
	"color",
	"display",
	"font-style",
	"font-weight",
	"list-style",
	"list-style-type",
	"text-align",
	"text-decoration",
	"white-space",
	NULL
};

bool
isGood(const char *param)
{
	int i;

	for (i = 0; good[i]; i++) {
		if (!strcmp(param, good[i])) {
			return true;
		}
	}

	return false;
}

#if 0
static std::string
trimString(std::string str)
{
	const std::string whiteSpaces = " \t\n\r\f\v";
	// Remove leading whitespace
	size_t first_non_space = str.find_first_not_of(whiteSpaces);
	str.erase(0, first_non_space);
	// Remove trailing whitespace
	size_t last_non_space = str.find_last_not_of(whiteSpaces);
	str.erase(last_non_space + 1);

	return str;
}
#endif

char *
trimString(char *str)
{
	char *whitespace = " \t\n\r\f\v";
	char *end = strchr(str, '\0') - 1;

	str += strspn(str, whitespace);
	while (end > str) {
		char *s;
		int bad = 0;
		for (s = whitespace; *s; s++) {
			if (*end == *s) {
				bad = 1;
				*end = '\0';
				break;
			}
		}
		if (!bad) {
			break;
		}
		end--;
	}
	return str;
}

void *
set_elstyle(const char *text)
{
	char *str, *param, *value, *next, *next2;
	struct hash *css = NULL;

	if (!text || !*text) {
		return NULL;
	}
	str = stracpy(text);
	next = str;

	while (next) {
		char *semicolon = strchr(str, ';');
		char *colon;
		char *params;

		if (semicolon) {
			*semicolon = '\0';
			next2 = semicolon + 1;
		} else {
			next2 = NULL;
		}
		params = next;
		colon = strchr(params, ':');

		if (!colon) {
			goto next_iter;
		}
		*colon = '\0';
		param = trimString(params);
		params = colon + 1;
		colon = strchr(params, ':');

		if (colon) {
			*colon = '\0';
		}
		value = trimString(params);

		if (isGood(param)) {
			if (!css) {
				css = init_hash8();
			}
			if (css) {
				char *key = stracpy(param);

				if (key) {
					add_hash_item(css, key, strlen(key), stracpy(value));
				}
			}
		}
next_iter:
		next = next2;
	}

	return (void *)css;
}

char *
get_elstyle(void *m)
{
	struct hash *css = (struct hash *)m;
	char *delimiter = "";
	struct hash_item *item;
	struct string output;
	int i;

	if (!init_string(&output)) {
		delete_map_str(css);
		return NULL;
	}

	foreach_hash_item (item, *css, i) {
		add_to_string(&output, delimiter);
		add_to_string(&output, item->key);
		add_char_to_string(&output, ':');
		add_to_string(&output, item->value);
		delimiter = ";";
	}
	delete_map_str(css);

	return output.source;
}

char *
get_css_value(const char *text, const char *param)
{
	void *m = set_elstyle(text);
	char *res = NULL;
	struct hash *css;
	struct hash_item *item;

	if (!m) {
		return stracpy("");
	}
	css = (struct hash *)m;
	item = get_hash_item(css, param, strlen(param));

	if (item) {
		res = stracpy(item->value);
	} else {
		res = stracpy("");
	}
	delete_map_str(css);

	return res;
}

char *
set_css_value(const char *text, const char *param, const char *value)
{
	void *m = set_elstyle(text);

	if (m) {
		if (isGood(param)) {
			struct hash *hash = (struct hash *)m;
			char *key = stracpy(param);

			if (key) {
				add_hash_item(hash, key, strlen(key), stracpy(value));
			}
		}
		return get_elstyle(m);
	}

	if (isGood(param)) {
		struct hash *css = init_hash8();
		char *key;

		if (!css) {
			return stracpy("");
		}
		key = stracpy(param);

		if (key) {
			add_hash_item(css, key, strlen(key), stracpy(value));
		}
		return get_elstyle((void *)css);
	}
	return stracpy("");
}
