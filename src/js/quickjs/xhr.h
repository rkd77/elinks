#ifndef EL__JS_QUICKJS_XHR_H
#define EL__JS_QUICKJS_XHR_H

#include <quickjs.h>
#include "session/download.h"
#include "util/lists.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    XHR_EVENT_ABORT = 0,
    XHR_EVENT_ERROR,
    XHR_EVENT_LOAD,
    XHR_EVENT_LOAD_END,
    XHR_EVENT_LOAD_START,
    XHR_EVENT_PROGRESS,
    XHR_EVENT_READY_STATE_CHANGED,
    XHR_EVENT_TIMEOUT,
    XHR_EVENT_MAX,
};

enum {
    XHR_RSTATE_UNSENT = 0,
    XHR_RSTATE_OPENED,
    XHR_RSTATE_HEADERS_RECEIVED,
    XHR_RSTATE_LOADING,
    XHR_RSTATE_DONE,
};

enum {
    XHR_RTYPE_DEFAULT = 0,
    XHR_RTYPE_TEXT,
    XHR_RTYPE_ARRAY_BUFFER,
    XHR_RTYPE_JSON,
};

struct emcascript_interpreter;

struct xhr_listener {
	LIST_HEAD_EL(struct xhr_listener);
	char *typ;
	JSValue fun;
};

struct Xhr {
//	std::map<std::string, std::string> requestHeaders;
//	std::map<std::string, std::string, classcomp> responseHeaders;
	void *requestHeaders;
	void *responseHeaders;

	struct download download;
	struct ecmascript_interpreter *interpreter;

	LIST_OF(struct xhr_listener) listeners;

	JSValue events[XHR_EVENT_MAX];
	JSValue thisVal;
	struct uri *uri;
	bool sent;
	bool async;
	short response_type;
	unsigned long timeout;
	unsigned short ready_state;

	int status;
	char *status_text;
	char *response_text;
	size_t response_length;

	struct {
		JSValue url;
		JSValue headers;
		JSValue response;
	} result;

	char *responseURL;
	int method;
};

int js_xhr_init(JSContext *ctx);
char *normalize(char *value);

#ifdef __cplusplus
}
#endif

#endif
