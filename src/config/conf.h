#ifndef EL__CONFIG_CONF_H
#define EL__CONFIG_CONF_H

#include "terminal/terminal.h"
#include "util/string.h"

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
enum parse_error parse_config_exmode_command(unsigned char *cmd);
#endif
void parse_config_file(struct option *options, unsigned char *name,
		       unsigned char *file, struct string *mirror,
		       int is_system_conf);
int write_config(struct terminal *);

unsigned char *
create_config_string(unsigned char *prefix, unsigned char *name,
		     struct option *options);

struct string *wrap_option_desc(struct string *out, const unsigned char *src,
				const struct string *indent, int maxwidth);

#endif
