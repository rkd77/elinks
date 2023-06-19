#ifndef EL__ECMASCRIPT_MUJS_XHR_H
#define EL__ECMASCRIPT_MUJS_XHR_H

#include <mujs.h>
#include "session/download.h"
#include "util/lists.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    GET = 1,
    HEAD = 2,
    POST = 3
};

struct xhr_listener {
	LIST_HEAD_EL(struct xhr_listener);
	char *typ;
	const char *fun;
};

struct mjs_xhr {
	//std::map<std::string, std::string> requestHeaders;
	//std::map<std::string, std::string, classcomp> responseHeaders;

	void *requestHeaders;
	void *responseHeaders;


	struct download download;
	struct ecmascript_interpreter *interpreter;

	LIST_OF(struct xhr_listener) listeners;

	const char *thisval;
	const char *onabort;
	const char *onerror;
	const char *onload;
	const char *onloadend;
	const char *onloadstart;
	const char *onprogress;
	const char *onreadystatechange;
	const char *ontimeout;
	struct uri *uri;
	char *response;
	char *responseText;
	char *responseType;
	char *responseURL;
	char *statusText;
	char *upload;
	bool async;
	bool withCredentials;
	bool isSend;
	bool isUpload;
	int method;
	int status;
	int timeout;
	size_t responseLength;
	unsigned short readyState;
};

int mjs_xhr_init(js_State *J);
char *normalize(char *value);

#ifdef __cplusplus
}
#endif

#endif
