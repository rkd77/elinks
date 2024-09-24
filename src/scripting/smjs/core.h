#ifndef EL__SCRIPTING_SMJS_CORE_H
#define EL__SCRIPTING_SMJS_CORE_H

#include "js/spidermonkey-shared.h"

struct module;
struct session;
struct string;

extern JSContext *smjs_ctx;
extern struct session *smjs_ses;

void alert_smjs_error(const char *msg);

void init_smjs(struct module *module);
void cleanup_smjs(struct module *module);

JSString *utf8_to_jsstring(JSContext *ctx, const char *str,
			   int length);
char *jsstring_to_utf8(JSContext *ctx, JSString *jsstr,
				int *length);

#endif
