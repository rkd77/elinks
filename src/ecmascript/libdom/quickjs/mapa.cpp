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

#include "ecmascript/libdom/quickjs/mapa.h"
#include "ecmascript/quickjs.h"
#include "ecmascript/quickjs/xhr.h"
#include "util/memory.h"
#include "util/string.h"

void
attr_save_in_map(void *m, void *node, JSValueConst value)
{
	std::map<void *, JSValueConst> *mapa = static_cast<std::map<void *, JSValueConst> *>(m);
	(*mapa)[node] = value;
}

void attr_save_in_map_void(void *m, void *node, void *value)
{
	std::map<void *, void *> *mapa = static_cast<std::map<void *, void *> *>(m);
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
attr_create_new_doctypes_map(void)
{
	std::map<void *, JSValueConst> *mapa = new std::map<void *, JSValueConst>;

	return (void *)mapa;
}

void *
attr_create_new_elements_map(void)
{
	std::map<void *, JSValueConst> *mapa = new std::map<void *, JSValueConst>;

	return (void *)mapa;
}

void *
attr_create_new_privates_map_void(void)
{
	std::map<void *, void *> *mapa = new std::map<void *, void *>;

	return (void *)mapa;
}

void *
attr_create_new_form_elements_map(void)
{
	std::map<void *, JSValueConst> *mapa = new std::map<void *, JSValueConst>;

	return (void *)mapa;
}

void *
attr_create_new_form_elements_map_rev(void)
{
	std::map<JSValueConst, void *> *mapa = new std::map<JSValueConst, void *>;

	return (void *)mapa;
}

void *
attr_create_new_form_map(void)
{
	std::map<void *, JSValueConst> *mapa = new std::map<void *, JSValueConst>;

	return (void *)mapa;
}

void *
attr_create_new_form_map_rev(void)
{
	std::map<JSValueConst, void *> *mapa = new std::map<JSValueConst, void *>;

	return (void *)mapa;
}

void *
attr_create_new_forms_map(void)
{
	std::map<void *, JSValueConst> *mapa = new std::map<void *, JSValueConst>;

	return (void *)mapa;
}

void *
attr_create_new_forms_map_rev(void)
{
	std::map<JSValueConst, void *> *mapa = new std::map<JSValueConst, void *>;

	return (void *)mapa;
}

void *
attr_create_new_input_map(void)
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

void *
attr_create_new_nodelist_map(void)
{
	std::map<void *, JSValueConst> *mapa = new std::map<void *, JSValueConst>;

	return (void *)mapa;
}

struct classcomp {
	bool operator() (const std::string& lhs, const std::string& rhs) const
	{
		return strcasecmp(lhs.c_str(), rhs.c_str()) < 0;
	}
};

void *
attr_create_new_requestHeaders_map(void)
{
	std::map<std::string, std::string> *mapa = new std::map<std::string, std::string>;

	return (void *)mapa;
}

void *
attr_create_new_responseHeaders_map(void)
{
	std::map<std::string, std::string, classcomp> *mapa = new std::map<std::string, std::string, classcomp>;

	return (void *)mapa;
}

void *
attr_create_new_nodelist_map_rev(void)
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

void
attr_clear_map_rev(void *m)
{
	std::map<JSValueConst, void *> *mapa = static_cast<std::map<JSValueConst, void *> *>(m);
	mapa->clear();
}

void
attr_clear_map_void(void *m)
{
	std::map<void *, void *> *mapa = static_cast<std::map<void *, void *> *>(m);
	mapa->clear();
}


void
attr_clear_map_str(void *m)
{
	std::map<std::string, std::string> *mapa = static_cast<std::map<std::string, std::string> *>(m);
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
	std::map<void *, JSValueConst> *mapa = static_cast<std::map<void *, JSValueConst> *>(m);

	if (mapa) {
		delete(mapa);
	}
}

void
attr_delete_map_rev(void *m)
{
	std::map<JSValueConst, void *> *mapa = static_cast<std::map<JSValueConst, void *> *>(m);

	if (mapa) {
		delete(mapa);
	}
}

void
attr_delete_map_void(void *m)
{
	std::map<void *, void *> *mapa = static_cast<std::map<void *, void *> *>(m);

	if (mapa) {
		delete(mapa);
	}
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

void *
attr_find_in_map_void(void *m, void *node)
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
	std::map<void *, JSValueConst> *mapa = static_cast<std::map<void *, JSValueConst> *>(m);
	mapa->erase(node);
}

void
attr_save_in_map_rev(void *m, JSValueConst value, void *node)
{
	std::map<JSValueConst, void *> *mapa = static_cast<std::map<JSValueConst, void *> *>(m);
	(*mapa)[value] = node;
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

void
attr_erase_from_map_rev(void *m, JSValueConst value)
{
	std::map<JSValueConst, void *> *mapa = static_cast<std::map<JSValueConst, void *> *>(m);
	mapa->erase(value);
}

static const std::vector<std::string>
explode(const std::string& s, const char& c)
{
	std::string buff("");
	std::vector<std::string> v;

	bool found = false;
	for (auto n:s) {
		if (found) {
			buff += n;
			continue;
		}
		if (n != c) {
			buff += n;
		}
		else if (n == c && buff != "") {
			v.push_back(buff);
			buff = "";
			found = true;
		}
	}

	if (buff != "") {
		v.push_back(buff);
	}

	return v;
}

static std::map<std::string, std::string> *
get_requestHeaders(void *h)
{
	return static_cast<std::map<std::string, std::string> *>(h);
}

static std::map<std::string, std::string, classcomp> *
get_responseHeaders(void *h)
{
	return static_cast<std::map<std::string, std::string, classcomp> *>(h);
}

void
process_xhr_headers(char *head, struct Xhr *x)
{
	std::istringstream headers(head);
	std::string http;
	int status;
	std::string statusText;
	std::string line;
	std::getline(headers, line);
	std::istringstream linestream(line);
	linestream >> http >> status >> statusText;

	auto responseHeaders = get_responseHeaders(x->responseHeaders);

	while (1) {
		std::getline(headers, line);

		if (line.empty()) {
			break;
		}
		std::vector<std::string> v = explode(line, ':');

		if (v.size() == 2) {
			char *value = stracpy(v[1].c_str());

			if (!value) {
				continue;
			}
			char *header = stracpy(v[0].c_str());

			if (!header) {
				mem_free(value);
				continue;
			}
			char *normalized_value = normalize(value);
			bool found = false;

			for (auto h: *responseHeaders) {
				const std::string hh = h.first;

				if (!strcasecmp(hh.c_str(), header)) {
					(*responseHeaders)[hh] = (*responseHeaders)[hh] + ", " + normalized_value;
					found = true;
					break;
				}
			}

			if (!found) {
				(*responseHeaders)[header] = normalized_value;
			}
			mem_free(header);
			mem_free(value);
		}
	}
	x->status = status;
	mem_free_set(&x->status_text, stracpy(statusText.c_str()));
}

void
set_xhr_header(char *normalized_value, const char *h_name, struct Xhr *x)
{
	bool found = false;
	auto requestHeaders = get_requestHeaders(x->requestHeaders);

	for (auto h: *requestHeaders) {
		const std::string hh = h.first;

		if (!strcasecmp(hh.c_str(), h_name)) {
			(*requestHeaders)[hh] = (*requestHeaders)[hh] + ", " + normalized_value;
			found = true;
			break;
		}
	}

	if (!found) {
		(*requestHeaders)[h_name] = normalized_value;
	}
}

const char *
get_output_headers(struct Xhr *x)
{
	std::string output = "";
	auto responseHeaders = get_responseHeaders(x->responseHeaders);

	for (auto h: *responseHeaders) {
		output += h.first + ": " + h.second + "\r\n";
	}

	return output.c_str();
}

const char *
get_output_header(const char *header_name, struct Xhr *x)
{
	std::string output = "";
	auto responseHeaders = get_responseHeaders(x->responseHeaders);

	for (auto h: *responseHeaders) {
		if (!strcasecmp(header_name, h.first.c_str())) {
			output = h.second;
			break;
		}
	}

	if (!output.empty()) {
		return output.c_str();
	}

	return NULL;
}
