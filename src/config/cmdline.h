#ifndef EL__CONFIG_CMDLINE_H
#define EL__CONFIG_CMDLINE_H

#include "main/main.h"
#include "util/lists.h"

enum retval parse_options(int, unsigned char *[],
			  LIST_OF(struct string_list_item) *);

#endif
