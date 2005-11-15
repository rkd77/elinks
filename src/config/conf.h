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
enum parse_error parse_config_command(struct option *options,
				      unsigned char **file, int *line,
				      struct string *mirror,
				      int is_system_conf);
void parse_config_file(struct option *options, unsigned char *name,
		       unsigned char *file, struct string *mirror,
		       int is_system_conf);
int write_config(struct terminal *);

unsigned char *
create_config_string(unsigned char *prefix, unsigned char *name,
		     struct option *options);

#endif
