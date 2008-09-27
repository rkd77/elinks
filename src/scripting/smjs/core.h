#ifndef EL__SCRIPTING_SMJS_CORE_H
#define EL__SCRIPTING_SMJS_CORE_H

#include "ecmascript/spidermonkey-shared.h"

struct module;
struct session;
struct string;

extern JSContext *smjs_ctx;
extern struct session *smjs_ses;

void smjs_error_reporter(JSContext *ctx, const char *message, JSErrorReport *report);
void alert_smjs_error(unsigned char *msg);

void init_smjs(struct module *module);
void cleanup_smjs(struct module *module);

#endif
