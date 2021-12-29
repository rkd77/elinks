#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include "terminal/terminal.h"
#include "util/string.h"

#ifdef __cplusplus
extern "C" {
#endif

enum parse_error {
	ERROR_NONE,
	ERROR_COMMAND,
	ERROR_PARSE,
	ERROR_OPTION,
	ERROR_VALUE,
	ERROR_NOMEM,
};

void load_config(void);
#ifdef CONFIG_EXMODE
enum parse_error parse_config_exmode_command(char *cmd);
#endif
void parse_config_file(struct option *options, char *name,
		       char *file, struct string *mirror,
		       int is_system_conf);
int write_config(struct terminal *);

char *create_config_string(char *prefix, char *name);

char *create_about_config_string(void);

struct string *wrap_option_desc(struct string *out, const char *src,
				const struct string *indent, int maxwidth);

#ifdef __cplusplus
}
#endif

#endif
