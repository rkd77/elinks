
#ifndef EL__SCRIPTING_SEE_CORE_H
#define EL__SCRIPTING_SEE_CORE_H

#include <see/see.h>

struct module;
struct session;
struct string;

extern struct SEE_interpreter see_interpreter;

struct string *convert_see_string(struct string *string, struct SEE_string *source);
void alert_see_error(struct session *ses, unsigned char *msg);
void see_report_error(struct session *ses, int state);

void init_see(struct module *module);

#endif
