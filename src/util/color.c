/** Color parser
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "util/color.h"
#include "util/conv.h"
#include "util/fastfind.h"
#include "util/string.h"

struct color_spec {
	const char *name;
	color_T rgb;
};

static const struct color_spec color_specs[] = {
#include "util/color_s.inc"
#ifndef CONFIG_SMALL
#include "util/color.inc"
#endif
	{ NULL,	0}
};

#ifdef USE_FASTFIND

static const struct color_spec *internal_pointer;

static void
colors_list_reset(void)
{
	internal_pointer = color_specs;
}

/** Returns a pointer to a struct that contains
 * current key and data pointers and increment
 * internal pointer.
 * It returns NULL when key is NULL. */
static struct fastfind_key_value *
colors_list_next(void)
{
	static struct fastfind_key_value kv;

	if (!internal_pointer->name) return NULL;

	kv.key = (unsigned char *) internal_pointer->name;
	kv.data = (void *) internal_pointer; /* cast away const */

	internal_pointer++;

	return &kv;
}

static struct fastfind_index ff_colors_index
	= INIT_FASTFIND_INDEX("colors_lookup", colors_list_reset, colors_list_next);

#endif /* USE_FASTFIND */

void
init_colors_lookup(void)
{
#ifdef USE_FASTFIND
	fastfind_index(&ff_colors_index, FF_COMPRESS | FF_LOCALE_INDEP);
#endif
}

void
free_colors_lookup(void)
{
#ifdef USE_FASTFIND
	fastfind_done(&ff_colors_index);
#endif
}

int
decode_color(const unsigned char *str, int slen, color_T *color)
{
	if (*str == '#' && (slen == 7 || slen == 4)) {
		unsigned char buffer[7];
		unsigned char *end;
		color_T string_color;

		str++;

decode_hex_color:
		if (slen == 4) {
			/* Expand the short hex color format */
			buffer[0] = buffer[1] = str[0];
			buffer[2] = buffer[3] = str[1];
			buffer[4] = buffer[5] = str[2];
			buffer[6] = 0;
			str = buffer;
		}

		errno = 0;
		string_color = strtoul(str, (char **) &end, 16);
		if (!errno && (end == str + 6) && string_color <= 0xFFFFFF) {
			*color = string_color;
			return 0;
		}
	} else {
		const struct color_spec *cs;

#ifndef USE_FASTFIND
		for (cs = color_specs; cs->name; cs++)
			if (!c_strlcasecmp(cs->name, -1, str, slen))
				break;
#else
		cs = fastfind_search(&ff_colors_index, str, slen);
#endif
		if (cs && cs->name) {
			*color = cs->rgb;
			return 0;

		} else if (slen == 6 || slen == 3) {
			/* Check if the string is just the hexadecimal rgb
			 * color notation with the leading '#' missing and
			 * treat it as such. */
			int len = 0;

			while (len < slen && isxdigit(str[len])) len++;

			if (len == slen) goto decode_hex_color;
		}
	}

	return -1; /* Not found */
}

const unsigned char *
get_color_string(color_T color, unsigned char hexcolor[8])
{
	const struct color_spec *cs;

	for (cs = color_specs; cs->name; cs++)
		if (cs->rgb == color)
			return cs->name;

	color_to_string(color, hexcolor);
	return hexcolor;
}

void
color_to_string(color_T color, unsigned char str[8])
{
	str[0]='#';
	elinks_ulongcat(&str[1], NULL, (unsigned long) color, 6, '0', 16, 0);
}
