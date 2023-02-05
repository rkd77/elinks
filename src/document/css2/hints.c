/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <strings.h>

#include "utils/nsoption.h"
#include "utils/corestrings.h"
#include "utils/log.h"
#include "utils/nsurl.h"
#include "utils/utils.h"

#include "css/hints.h"
#include "css/select.h"

#define LOG_STATS
#undef LOG_STATS

/******************************************************************************
 * Utility functions                                                          *
 ******************************************************************************/

/**
 * Determine if a given character is whitespace
 *
 * \param c  Character to consider
 * \return true if character is whitespace, false otherwise
 */
static bool isWhitespace(char c)
{
	return c == ' ' || c == '\t' || c == '\f' || c == '\r' || c == '\n';
}

/**
 * Determine if a given character is a valid hex digit
 *
 * \param c  Character to consider
 * \return true if character is a valid hex digit, false otherwise
 */
static bool isHex(char c)
{
	return ('0' <= c && c <= '9') ||
			('A' <= (c & ~0x20) && (c & ~0x20) <= 'F');
}

/**
 * Convert a character representing a hex digit to the corresponding hex value
 *
 * \param c  Character to convert
 * \return Hex value represented by character
 *
 * \note This function assumes an ASCII-compatible character set
 */
static uint8_t charToHex(char c)
{
	/* 0-9 */
	c -= '0';

	/* A-F */
	if (c > 9)
		c -= 'A' - '9' - 1;

	/* a-f */
	if (c > 15)
		c -= 'a' - 'A';

	return c;
}


/******************************************************************************
 * Common parsing functions                                                   *
 ******************************************************************************/

/**
 * Parse a number string
 *
 * \param data  Data to parse (NUL-terminated)
 * \param maybe_negative  Negative numbers permitted
 * \param real            Floating point numbers permitted
 * \param value           Pointer to location to receive numeric value
 * \param consumed        Pointer to location to receive number of input
 *                        bytes consumed
 * \return true on success, false on invalid input
 */
static bool parse_number(const char *data, bool maybe_negative, bool real,
		css_fixed *value, size_t *consumed)
{
	size_t len;
	const uint8_t *ptr;
	int32_t intpart = 0;
	int32_t fracpart = 0;
	int32_t pwr = 1;
	int sign = 1;

	*consumed = 0;

	len = strlen(data);
	ptr = (const uint8_t *) data;

	if (len == 0)
		return false;

	/* Skip leading whitespace */
	while (len > 0 && isWhitespace(ptr[0])) {
		len--;
		ptr++;
	}

	if (len == 0)
		return false;

	/* Extract sign, if any */
	if (ptr[0] == '+') {
		len--;
		ptr++;
	} else if (ptr[0] == '-' && maybe_negative) {
		sign = -1;
		len--;
		ptr++;
	}

	if (len == 0)
		return false;

	/* Must have a digit [0,9] */
	if ('0' > ptr[0] || ptr[0] > '9')
		return false;

	/* Now extract intpart, assuming base 10 */
	while (len > 0) {
		/* Stop on first non-digit */
		if (ptr[0] < '0' || '9' < ptr[0])
			break;

		/* Prevent overflow of 'intpart'; proper clamping below */
		if (intpart < (1 << 22)) {
			intpart *= 10;
			intpart += ptr[0] - '0';
		}
		ptr++;
		len--;
	}

	/* And fracpart, again, assuming base 10 */
	if (real && len > 1 && ptr[0] == '.' &&
			('0' <= ptr[1] && ptr[1] <= '9')) {
		ptr++;
		len--;

		while (len > 0) {
			if (ptr[0] < '0' || '9' < ptr[0])
				break;

			if (pwr < 1000000) {
				pwr *= 10;
				fracpart *= 10;
				fracpart += ptr[0] - '0';
			}
			ptr++;
			len--;
		}

		fracpart = ((1 << 10) * fracpart + pwr/2) / pwr;
		if (fracpart >= (1 << 10)) {
			intpart++;
			fracpart &= (1 << 10) - 1;
		}
	}

	if (sign > 0) {
		/* If the result is larger than we can represent,
		 * then clamp to the maximum value we can store. */
		if (intpart >= (1 << 21)) {
			intpart = (1 << 21) - 1;
			fracpart = (1 << 10) - 1;
		}
	} else {
		/* If the negated result is smaller than we can represent
		 * then clamp to the minimum value we can store. */
		if (intpart >= (1 << 21)) {
			intpart = -(1 << 21);
			fracpart = 0;
		} else {
			intpart = -intpart;
			if (fracpart) {
				fracpart = (1 << 10) - fracpart;
				intpart--;
			}
		}
	}

	*value = (intpart << 10) | fracpart;

	*consumed = ptr - (const uint8_t *) data;

	return true;
}

/**
 * Parse a dimension string
 *
 * \param data    Data to parse (NUL-terminated)
 * \param strict  Whether to enforce strict parsing rules
 * \param length  Pointer to location to receive dimension's length
 * \param unit    Pointer to location to receive dimension's unit
 * \return true on success, false on invalid input
 */
static bool parse_dimension(const char *data, bool strict, css_fixed *length,
		css_unit *unit)
{
	size_t len;
	size_t read;
	css_fixed value;

	len = strlen(data);

	if (parse_number(data, false, true, &value, &read) == false)
		return false;

	if (strict && value < INTTOFIX(1))
		return false;

	*length = value;

	if (len > read && data[read] == '%')
		*unit = CSS_UNIT_PCT;
	else
		*unit = CSS_UNIT_PX;

	return true;
}

/**
 * Mapping of colour name to CSS color
 */
struct colour_map {
	const char *name;
	css_color color;
};

/**
 * Name comparator for named colour matching
 *
 * \param a  Name to match
 * \param b  Colour map entry to consider
 * \return 0   on match,
 *         < 0 if a < b,
 *         > 0 if b > a.
 */
static int cmp_colour_name(const void *a, const void *b)
{
	const char *aa = a;
	const struct colour_map *bb = b;

	return strcasecmp(aa, bb->name);
}

/**
 * Parse a named colour
 *
 * \param name    Name to parse
 * \param result  Pointer to location to receive css_color
 * \return true on success, false on invalid input
 */
static bool parse_named_colour(const char *name, css_color *result)
{
	static const struct colour_map named_colours[] = {
		{ "aliceblue",		0xfff0f8ff },
		{ "antiquewhite",	0xfffaebd7 },
		{ "aqua",		0xff00ffff },
		{ "aquamarine",		0xff7fffd4 },
		{ "azure",		0xfff0ffff },
		{ "beige",		0xfff5f5dc },
		{ "bisque",		0xffffe4c4 },
		{ "black",		0xff000000 },
		{ "blanchedalmond",	0xffffebcd },
		{ "blue",		0xff0000ff },
		{ "blueviolet",		0xff8a2be2 },
		{ "brown",		0xffa52a2a },
		{ "burlywood",		0xffdeb887 },
		{ "cadetblue",		0xff5f9ea0 },
		{ "chartreuse",		0xff7fff00 },
		{ "chocolate",		0xffd2691e },
		{ "coral",		0xffff7f50 },
		{ "cornflowerblue",	0xff6495ed },
		{ "cornsilk",		0xfffff8dc },
		{ "crimson",		0xffdc143c },
		{ "cyan",		0xff00ffff },
		{ "darkblue",		0xff00008b },
		{ "darkcyan",		0xff008b8b },
		{ "darkgoldenrod",	0xffb8860b },
		{ "darkgray",		0xffa9a9a9 },
		{ "darkgreen",		0xff006400 },
		{ "darkgrey",		0xffa9a9a9 },
		{ "darkkhaki",		0xffbdb76b },
		{ "darkmagenta",	0xff8b008b },
		{ "darkolivegreen",	0xff556b2f },
		{ "darkorange",		0xffff8c00 },
		{ "darkorchid",		0xff9932cc },
		{ "darkred",		0xff8b0000 },
		{ "darksalmon",		0xffe9967a },
		{ "darkseagreen",	0xff8fbc8f },
		{ "darkslateblue",	0xff483d8b },
		{ "darkslategray",	0xff2f4f4f },
		{ "darkslategrey",	0xff2f4f4f },
		{ "darkturquoise",	0xff00ced1 },
		{ "darkviolet",		0xff9400d3 },
		{ "deeppink",		0xffff1493 },
		{ "deepskyblue",	0xff00bfff },
		{ "dimgray",		0xff696969 },
		{ "dimgrey",		0xff696969 },
		{ "dodgerblue",		0xff1e90ff },
		{ "feldspar",		0xffd19275 },
		{ "firebrick",		0xffb22222 },
		{ "floralwhite",	0xfffffaf0 },
		{ "forestgreen",	0xff228b22 },
		{ "fuchsia",		0xffff00ff },
		{ "gainsboro",		0xffdcdcdc },
		{ "ghostwhite",		0xfff8f8ff },
		{ "gold",		0xffffd700 },
		{ "goldenrod",		0xffdaa520 },
		{ "gray",		0xff808080 },
		{ "green",		0xff008000 },
		{ "greenyellow",	0xffadff2f },
		{ "grey",		0xff808080 },
		{ "honeydew",		0xfff0fff0 },
		{ "hotpink",		0xffff69b4 },
		{ "indianred",		0xffcd5c5c },
		{ "indigo",		0xff4b0082 },
		{ "ivory",		0xfffffff0 },
		{ "khaki",		0xfff0e68c },
		{ "lavender",		0xffe6e6fa },
		{ "lavenderblush",	0xfffff0f5 },
		{ "lawngreen",		0xff7cfc00 },
		{ "lemonchiffon",	0xfffffacd },
		{ "lightblue",		0xffadd8e6 },
		{ "lightcoral",		0xfff08080 },
		{ "lightcyan",		0xffe0ffff },
		{ "lightgoldenrodyellow",	0xfffafad2 },
		{ "lightgray",		0xffd3d3d3 },
		{ "lightgreen",		0xff90ee90 },
		{ "lightgrey",		0xffd3d3d3 },
		{ "lightpink",		0xffffb6c1 },
		{ "lightsalmon",	0xffffa07a },
		{ "lightseagreen",	0xff20b2aa },
		{ "lightskyblue",	0xff87cefa },
		{ "lightslateblue",	0xff8470ff },
		{ "lightslategray",	0xff778899 },
		{ "lightslategrey",	0xff778899 },
		{ "lightsteelblue",	0xffb0c4de },
		{ "lightyellow",	0xffffffe0 },
		{ "lime",		0xff00ff00 },
		{ "limegreen",		0xff32cd32 },
		{ "linen",		0xfffaf0e6 },
		{ "magenta",		0xffff00ff },
		{ "maroon",		0xff800000 },
		{ "mediumaquamarine",	0xff66cdaa },
		{ "mediumblue",		0xff0000cd },
		{ "mediumorchid",	0xffba55d3 },
		{ "mediumpurple",	0xff9370db },
		{ "mediumseagreen",	0xff3cb371 },
		{ "mediumslateblue",	0xff7b68ee },
		{ "mediumspringgreen",	0xff00fa9a },
		{ "mediumturquoise",	0xff48d1cc },
		{ "mediumvioletred",	0xffc71585 },
		{ "midnightblue",	0xff191970 },
		{ "mintcream",		0xfff5fffa },
		{ "mistyrose",		0xffffe4e1 },
		{ "moccasin",		0xffffe4b5 },
		{ "navajowhite",	0xffffdead },
		{ "navy",		0xff000080 },
		{ "oldlace",		0xfffdf5e6 },
		{ "olive",		0xff808000 },
		{ "olivedrab",		0xff6b8e23 },
		{ "orange",		0xffffa500 },
		{ "orangered",		0xffff4500 },
		{ "orchid",		0xffda70d6 },
		{ "palegoldenrod",	0xffeee8aa },
		{ "palegreen",		0xff98fb98 },
		{ "paleturquoise",	0xffafeeee },
		{ "palevioletred",	0xffdb7093 },
		{ "papayawhip",		0xffffefd5 },
		{ "peachpuff",		0xffffdab9 },
		{ "peru",		0xffcd853f },
		{ "pink",		0xffffc0cb },
		{ "plum",		0xffdda0dd },
		{ "powderblue",		0xffb0e0e6 },
		{ "purple",		0xff800080 },
		{ "red",		0xffff0000 },
		{ "rosybrown",		0xffbc8f8f },
		{ "royalblue",		0xff4169e1 },
		{ "saddlebrown",	0xff8b4513 },
		{ "salmon",		0xfffa8072 },
		{ "sandybrown",		0xfff4a460 },
		{ "seagreen",		0xff2e8b57 },
		{ "seashell",		0xfffff5ee },
		{ "sienna",		0xffa0522d },
		{ "silver",		0xffc0c0c0 },
		{ "skyblue",		0xff87ceeb },
		{ "slateblue",		0xff6a5acd },
		{ "slategray",		0xff708090 },
		{ "slategrey",		0xff708090 },
		{ "snow",		0xfffffafa },
		{ "springgreen",	0xff00ff7f },
		{ "steelblue",		0xff4682b4 },
		{ "tan",		0xffd2b48c },
		{ "teal",		0xff008080 },
		{ "thistle",		0xffd8bfd8 },
		{ "tomato",		0xffff6347 },
		{ "turquoise",		0xff40e0d0 },
		{ "violet",		0xffee82ee },
		{ "violetred",		0xffd02090 },
		{ "wheat",		0xfff5deb3 },
		{ "white",		0xffffffff },
		{ "whitesmoke",		0xfff5f5f5 },
		{ "yellow",		0xffffff00 },
		{ "yellowgreen",	0xff9acd32 }
	};
	const struct colour_map *entry;

	entry = bsearch(name, named_colours,
			sizeof(named_colours) / sizeof(named_colours[0]),
			sizeof(named_colours[0]),
			cmp_colour_name);

	if (entry != NULL)
		*result = entry->color;

	return entry != NULL;
}

/* exported interface documented in content/handlers/css/hints.h */
bool nscss_parse_colour(const char *data, css_color *result)
{
	size_t len = strlen(data);
	uint8_t r, g, b;

	/* 2 */
	if (len == 0)
		return false;

	/* 3 */
	if (len == SLEN("transparent") && strcasecmp(data, "transparent") == 0)
		return false;

	/* 4 */
	if (parse_named_colour(data, result))
		return true;

	/** \todo Implement HTML5's utterly insane legacy colour parsing */

	if (data[0] == '#') {
		data++;
		len--;
	}

	if (len == 3 && isHex(data[0]) && isHex(data[1]) && isHex(data[2])) {
		r = charToHex(data[0]);
		g = charToHex(data[1]);
		b = charToHex(data[2]);

		r |= (r << 4);
		g |= (g << 4);
		b |= (b << 4);

		*result = (0xff << 24) | (r << 16) | (g << 8) | b;

		return true;
	} else if (len == 6 && isHex(data[0]) && isHex(data[1]) &&
			isHex(data[2]) && isHex(data[3]) && isHex(data[4]) &&
			isHex(data[5])) {
		r = (charToHex(data[0]) << 4) | charToHex(data[1]);
		g = (charToHex(data[2]) << 4) | charToHex(data[3]);
		b = (charToHex(data[4]) << 4) | charToHex(data[5]);

		*result = (0xff << 24) | (r << 16) | (g << 8) | b;

		return true;
	}

	return false;
}

/**
 * Parse a font \@size attribute
 *
 * \param size  Data to parse (NUL-terminated)
 * \param val   Pointer to location to receive enum value
 * \param len   Pointer to location to receive length
 * \param unit  Pointer to location to receive unit
 * \return True on success, false on failure
 */
static bool parse_font_size(const char *size, uint8_t *val,
		css_fixed *len, css_unit *unit)
{
	static const uint8_t size_map[] = {
		CSS_FONT_SIZE_XX_SMALL,
		CSS_FONT_SIZE_SMALL,
		CSS_FONT_SIZE_MEDIUM,
		CSS_FONT_SIZE_LARGE,
		CSS_FONT_SIZE_X_LARGE,
		CSS_FONT_SIZE_XX_LARGE,
		CSS_FONT_SIZE_DIMENSION	/* xxx-large (see below) */
	};

	const char *p = size;
	char mode;
	int value = 0;

	/* Skip whitespace */
	while (*p != '\0' && isWhitespace(*p))
		p++;

	mode = *p;

	/* Skip +/- */
	if (mode == '+' || mode == '-')
		p++;

	/* Need at least one digit */
	if (*p < '0' || *p > '9') {
		return false;
	}

	/* Consume digits, computing value */
	while ('0' <= *p && *p <= '9') {
		value = value * 10 + (*p - '0');
		p++;
	}

	/* Resolve relative sizes */
	if (mode == '+')
		value += 3;
	else if (mode == '-')
		value = 3 - value;

	/* Clamp to range [1,7] */
	if (value < 1)
		value = 1;
	else if (value > 7)
		value = 7;

	if (value == 7) {
		/* Manufacture xxx-large */
	  *len = FDIV(FMUL(INTTOFIX(3), INTTOFIX(nsoption_int(font_size))),
				F_10);
	} else {
		/* Len is irrelevant */
		*len = 0;
	}

	*unit = CSS_UNIT_PT;
	*val = size_map[value - 1];

	return true;
}


/******************************************************************************
 * Hint context management                                                    *
 ******************************************************************************/

#define MAX_HINTS_PER_ELEMENT 32

struct css_hint_ctx {
	struct css_hint *hints;
	uint32_t len;
};

struct css_hint_ctx hint_ctx;

nserror css_hint_init(void)
{
	hint_ctx.hints = malloc(sizeof(struct css_hint) *
			MAX_HINTS_PER_ELEMENT);
	if (hint_ctx.hints == NULL) {
		return NSERROR_NOMEM;
	}

	return NSERROR_OK;
}

void css_hint_fini(void)
{
	hint_ctx.len = 0;
	free(hint_ctx.hints);
}

static void css_hint_clean(void)
{
	hint_ctx.len = 0;
}

static inline void css_hint_advance(struct css_hint **hint)
{
	hint_ctx.len++;
	assert(hint_ctx.len < MAX_HINTS_PER_ELEMENT);

	(*hint)++;
}

static void css_hint_get_hints(struct css_hint **hints, uint32_t *nhints)
{
	*hints = hint_ctx.hints;
	*nhints = hint_ctx.len;
}


/******************************************************************************
 * Presentational hint handlers                                               *
 ******************************************************************************/

static void css_hint_table_cell_border_padding(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	css_qname qs;
	dom_string *attr = NULL;
	dom_node *tablenode = NULL;
	dom_exception exc;

	qs.ns = NULL;
	qs.name = lwc_string_ref(corestring_lwc_table);
	if (named_ancestor_node(ctx, node, &qs,
			(void *)&tablenode) != CSS_OK) {
		/* Didn't find, or had error */
		lwc_string_unref(qs.name);
		return;
	}
	lwc_string_unref(qs.name);

	if (tablenode == NULL) {
		return;
	}
	/* No need to unref tablenode, named_ancestor_node does not
	 * return a reffed node to the CSS
	 */

	exc = dom_element_get_attribute(tablenode,
			corestring_dom_border, &attr);

	if (exc == DOM_NO_ERR && attr != NULL) {
		uint32_t hint_prop;
		css_hint_length hint_length;

		if (parse_dimension(
				dom_string_data(attr), false,
						&hint_length.value,
						&hint_length.unit) &&
				INTTOFIX(0) != hint_length.value) {

			for (hint_prop = CSS_PROP_BORDER_TOP_STYLE;
			     hint_prop <= CSS_PROP_BORDER_LEFT_STYLE;
			     hint_prop++) {
				hint->prop = hint_prop;
				hint->status = CSS_BORDER_STYLE_INSET;
				css_hint_advance(&hint);
			}

			for (hint_prop = CSS_PROP_BORDER_TOP_WIDTH;
			     hint_prop <= CSS_PROP_BORDER_LEFT_WIDTH;
			     hint_prop++) {
				hint->prop = hint_prop;
				hint->data.length.value = INTTOFIX(1);
				hint->data.length.unit = CSS_UNIT_PX;
				hint->status = CSS_BORDER_WIDTH_WIDTH;
				css_hint_advance(&hint);
			}
		}
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(tablenode,
			corestring_dom_bordercolor, &attr);

	if (exc == DOM_NO_ERR && attr != NULL) {
		uint32_t hint_prop;
		css_color hint_color;

		if (nscss_parse_colour(
				(const char *)dom_string_data(attr),
				&hint_color)) {

			for (hint_prop = CSS_PROP_BORDER_TOP_COLOR;
			     hint_prop <= CSS_PROP_BORDER_LEFT_COLOR;
			     hint_prop++) {
				hint->prop = hint_prop;
				hint->data.color = hint_color;
				hint->status = CSS_BORDER_COLOR_COLOR;
				css_hint_advance(&hint);
			}
		}
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(tablenode,
			corestring_dom_cellpadding, &attr);

	if (exc == DOM_NO_ERR && attr != NULL) {
		uint32_t hint_prop;
		css_hint_length hint_length;

		if (parse_dimension(
				dom_string_data(attr), false,
						&hint_length.value,
						&hint_length.unit)) {

			for (hint_prop = CSS_PROP_PADDING_TOP;
			     hint_prop <= CSS_PROP_PADDING_LEFT;
			     hint_prop++) {
				hint->prop = hint_prop;
				hint->data.length.value = hint_length.value;
				hint->data.length.unit = hint_length.unit;
				hint->status = CSS_PADDING_SET;
				css_hint_advance(&hint);
			}
		}
		dom_string_unref(attr);
	}
}

static void css_hint_vertical_align_table_cells(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_string *attr = NULL;
	dom_exception err;

	err = dom_element_get_attribute(node,
			corestring_dom_valign, &attr);

	if (err == DOM_NO_ERR && attr != NULL) {
		hint->data.length.value = 0;
		hint->data.length.unit = CSS_UNIT_PX;
		if (dom_string_caseless_lwc_isequal(attr,
				corestring_lwc_top)) {
			hint->prop = CSS_PROP_VERTICAL_ALIGN;
			hint->status = CSS_VERTICAL_ALIGN_TOP;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(attr,
					corestring_lwc_middle)) {
			hint->prop = CSS_PROP_VERTICAL_ALIGN;
			hint->status = CSS_VERTICAL_ALIGN_MIDDLE;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(attr,
					corestring_lwc_bottom)) {
			hint->prop = CSS_PROP_VERTICAL_ALIGN;
			hint->status = CSS_VERTICAL_ALIGN_BOTTOM;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(attr,
					corestring_lwc_baseline)) {
			hint->prop = CSS_PROP_VERTICAL_ALIGN;
			hint->status = CSS_VERTICAL_ALIGN_BASELINE;
			css_hint_advance(&hint);
		}
		dom_string_unref(attr);
	}
}

static void css_hint_vertical_align_replaced(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_string *attr = NULL;
	dom_exception err;

	err = dom_element_get_attribute(node,
			corestring_dom_valign, &attr);

	if (err == DOM_NO_ERR && attr != NULL) {
		if (dom_string_caseless_lwc_isequal(attr,
				corestring_lwc_top)) {
			hint->prop = CSS_PROP_VERTICAL_ALIGN;
			hint->status = CSS_VERTICAL_ALIGN_TOP;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(attr,
					corestring_lwc_bottom) ||
			   dom_string_caseless_lwc_isequal(attr,
					corestring_lwc_baseline)) {
			hint->prop = CSS_PROP_VERTICAL_ALIGN;
			hint->status = CSS_VERTICAL_ALIGN_BASELINE;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(attr,
					corestring_lwc_texttop)) {
			hint->prop = CSS_PROP_VERTICAL_ALIGN;
			hint->status = CSS_VERTICAL_ALIGN_TEXT_TOP;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(attr,
					corestring_lwc_absmiddle) ||
			   dom_string_caseless_lwc_isequal(attr,
					corestring_lwc_abscenter)) {
			hint->prop = CSS_PROP_VERTICAL_ALIGN;
			hint->status = CSS_VERTICAL_ALIGN_MIDDLE;
			css_hint_advance(&hint);
		}
		dom_string_unref(attr);
	}
}

static void css_hint_text_align_normal(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_string *align = NULL;
	dom_exception err;

	err = dom_element_get_attribute(node,
			corestring_dom_align, &align);
	if (err == DOM_NO_ERR && align != NULL) {
		if (dom_string_caseless_lwc_isequal(align,
				corestring_lwc_left)) {
			hint->prop = CSS_PROP_TEXT_ALIGN;
			hint->status = CSS_TEXT_ALIGN_LEFT;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(align,
					corestring_lwc_center)) {
			hint->prop = CSS_PROP_TEXT_ALIGN;
			hint->status = CSS_TEXT_ALIGN_CENTER;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(align,
					corestring_lwc_right)) {
			hint->prop = CSS_PROP_TEXT_ALIGN;
			hint->status = CSS_TEXT_ALIGN_RIGHT;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(align,
					corestring_lwc_justify)) {
			hint->prop = CSS_PROP_TEXT_ALIGN;
			hint->status = CSS_TEXT_ALIGN_JUSTIFY;
			css_hint_advance(&hint);
		}
		dom_string_unref(align);
	}
}

static void css_hint_text_align_center(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];

	hint->prop = CSS_PROP_TEXT_ALIGN;
	hint->status = CSS_TEXT_ALIGN_LIBCSS_CENTER;
	css_hint_advance(&hint);
}

static void css_hint_margin_left_right_align_center(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_string *attr;
	dom_exception exc;

	exc = dom_element_get_attribute(node,
			corestring_dom_align, &attr);

	if (exc == DOM_NO_ERR && attr != NULL) {
		memset(hint, 0, sizeof(*hint) * 2);
		if (dom_string_caseless_lwc_isequal(attr,
						corestring_lwc_center) ||
				dom_string_caseless_lwc_isequal(attr,
						corestring_lwc_abscenter) ||
				dom_string_caseless_lwc_isequal(attr,
						corestring_lwc_middle) ||
				dom_string_caseless_lwc_isequal(attr,
						corestring_lwc_absmiddle)) {
			hint->prop = CSS_PROP_MARGIN_LEFT;
			hint->status = CSS_MARGIN_AUTO;
			css_hint_advance(&hint);

			hint->prop = CSS_PROP_MARGIN_RIGHT;
			hint->status = CSS_MARGIN_AUTO;
			css_hint_advance(&hint);
		}
		dom_string_unref(attr);
	}
}

static void css_hint_text_align_special(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_string *align = NULL;
	dom_exception err;

	err = dom_element_get_attribute(node,
			corestring_dom_align, &align);

	if (err == DOM_NO_ERR && align != NULL) {
		if (dom_string_caseless_lwc_isequal(align,
				corestring_lwc_center)) {
			hint->prop = CSS_PROP_TEXT_ALIGN;
			hint->status = CSS_TEXT_ALIGN_LIBCSS_CENTER;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(align,
					corestring_lwc_left)) {
			hint->prop = CSS_PROP_TEXT_ALIGN;
			hint->status = CSS_TEXT_ALIGN_LIBCSS_LEFT;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(align,
					corestring_lwc_right)) {
			hint->prop = CSS_PROP_TEXT_ALIGN;
			hint->status = CSS_TEXT_ALIGN_LIBCSS_RIGHT;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(align,
					corestring_lwc_justify)) {
			hint->prop = CSS_PROP_TEXT_ALIGN;
			hint->status = CSS_TEXT_ALIGN_JUSTIFY;
			css_hint_advance(&hint);
		}
		dom_string_unref(align);
	}
}

static void css_hint_text_align_table_special(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];

	hint->prop = CSS_PROP_TEXT_ALIGN;
	hint->status = CSS_TEXT_ALIGN_INHERIT_IF_NON_MAGIC;
	css_hint_advance(&hint);
}

static void css_hint_margin_hspace_vspace(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_string *attr = NULL;
	dom_exception exc;

	exc = dom_element_get_attribute(node,
			corestring_dom_vspace, &attr);

	if (exc == DOM_NO_ERR && attr != NULL) {
		css_hint_length hint_length;
		if (parse_dimension(
				dom_string_data(attr), false,
				&hint_length.value,
				&hint_length.unit)) {
			hint->prop = CSS_PROP_MARGIN_TOP;
			hint->data.length.value = hint_length.value;
			hint->data.length.unit = hint_length.unit;
			hint->status = CSS_MARGIN_SET;
			css_hint_advance(&hint);

			hint->prop = CSS_PROP_MARGIN_BOTTOM;
			hint->data.length.value = hint_length.value;
			hint->data.length.unit = hint_length.unit;
			hint->status = CSS_MARGIN_SET;
			css_hint_advance(&hint);
		}
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(node,
			corestring_dom_hspace, &attr);

	if (exc == DOM_NO_ERR && attr != NULL) {
		css_hint_length hint_length;
		if (parse_dimension(
				dom_string_data(attr), false,
				&hint_length.value,
				&hint_length.unit)) {
			hint->prop = CSS_PROP_MARGIN_LEFT;
			hint->data.length.value = hint_length.value;
			hint->data.length.unit = hint_length.unit;
			hint->status = CSS_MARGIN_SET;
			css_hint_advance(&hint);

			hint->prop = CSS_PROP_MARGIN_RIGHT;
			hint->data.length.value = hint_length.value;
			hint->data.length.unit = hint_length.unit;
			hint->status = CSS_MARGIN_SET;
			css_hint_advance(&hint);
		}
		dom_string_unref(attr);
	}
}

static void css_hint_margin_left_right_hr(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_string *attr;
	dom_exception exc;

	exc = dom_element_get_attribute(node,
				corestring_dom_align, &attr);

	if (exc == DOM_NO_ERR && attr != NULL) {
		memset(hint, 0, sizeof(*hint) * 2);
		if (dom_string_caseless_lwc_isequal(attr,
				corestring_lwc_left)) {
			hint->prop = CSS_PROP_MARGIN_LEFT;
			hint->data.length.value = 0;
			hint->data.length.unit = CSS_UNIT_PX;
			hint->status = CSS_MARGIN_SET;
			css_hint_advance(&hint);

			hint->prop = CSS_PROP_MARGIN_RIGHT;
			hint->status = CSS_MARGIN_AUTO;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(attr,
				corestring_lwc_center)) {
			hint->prop = CSS_PROP_MARGIN_LEFT;
			hint->status = CSS_MARGIN_AUTO;
			css_hint_advance(&hint);

			hint->prop = CSS_PROP_MARGIN_RIGHT;
			hint->status = CSS_MARGIN_AUTO;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(attr,
				corestring_lwc_right)) {
			hint->prop = CSS_PROP_MARGIN_LEFT;
			hint->status = CSS_MARGIN_AUTO;
			css_hint_advance(&hint);

			hint->prop = CSS_PROP_MARGIN_RIGHT;
			hint->data.length.value = 0;
			hint->data.length.unit = CSS_UNIT_PX;
			hint->status = CSS_MARGIN_SET;
			css_hint_advance(&hint);
		}
		dom_string_unref(attr);
	}
}

static void css_hint_table_spacing_border(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_exception exc;
	dom_string *attr = NULL;

	exc = dom_element_get_attribute(node, corestring_dom_border, &attr);

	if (exc == DOM_NO_ERR && attr != NULL) {
		uint32_t hint_prop;
		css_hint_length hint_length;

		for (hint_prop = CSS_PROP_BORDER_TOP_STYLE;
			     hint_prop <= CSS_PROP_BORDER_LEFT_STYLE;
			     hint_prop++) {
				hint->prop = hint_prop;
				hint->status = CSS_BORDER_STYLE_OUTSET;
				css_hint_advance(&hint);
		}

		if (parse_dimension(
				dom_string_data(attr), false,
						&hint_length.value,
						&hint_length.unit)) {

			for (hint_prop = CSS_PROP_BORDER_TOP_WIDTH;
			     hint_prop <= CSS_PROP_BORDER_LEFT_WIDTH;
			     hint_prop++) {
				hint->prop = hint_prop;
				hint->data.length.value = hint_length.value;
				hint->data.length.unit = hint_length.unit;
				hint->status = CSS_BORDER_WIDTH_WIDTH;
				css_hint_advance(&hint);
			}
		}
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(node,
			corestring_dom_bordercolor, &attr);

	if (exc == DOM_NO_ERR && attr != NULL) {
		uint32_t hint_prop;
		css_color hint_color;

		if (nscss_parse_colour(
				(const char *)dom_string_data(attr),
				&hint_color)) {

			for (hint_prop = CSS_PROP_BORDER_TOP_COLOR;
			     hint_prop <= CSS_PROP_BORDER_LEFT_COLOR;
			     hint_prop++) {
				hint->prop = hint_prop;
				hint->data.color = hint_color;
				hint->status = CSS_BORDER_COLOR_COLOR;
				css_hint_advance(&hint);
			}
		}
		dom_string_unref(attr);
	}

	exc = dom_element_get_attribute(node,
			corestring_dom_cellspacing, &attr);

	if (exc == DOM_NO_ERR && attr != NULL) {
		if (parse_dimension(
				(const char *)dom_string_data(attr), false,
						&hint->data.position.h.value,
						&hint->data.position.h.unit)) {
			hint->prop = CSS_PROP_BORDER_SPACING;
			hint->data.position.v = hint->data.position.h;
			hint->status = CSS_BORDER_SPACING_SET;
			css_hint_advance(&hint);
		}
		dom_string_unref(attr);
	}
}

static void css_hint_height(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_string *attr = NULL;
	dom_exception err;

	err = dom_element_get_attribute(node,
			corestring_dom_height, &attr);

	if (err == DOM_NO_ERR && attr != NULL) {
		if (parse_dimension(
				(const char *)dom_string_data(attr), false,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			hint->prop = CSS_PROP_HEIGHT;
			hint->status = CSS_HEIGHT_SET;
			css_hint_advance(&hint);
		}
		dom_string_unref(attr);
	}
}

static void css_hint_width(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_string *attr = NULL;
	dom_exception err;

	err = dom_element_get_attribute(node,
			corestring_dom_width, &attr);

	if (err == DOM_NO_ERR && attr != NULL) {
		if (parse_dimension(
				(const char *)dom_string_data(attr), false,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			hint->prop = CSS_PROP_WIDTH;
			hint->status = CSS_WIDTH_SET;
			css_hint_advance(&hint);
		}
		dom_string_unref(attr);
	}
}

static void css_hint_height_width_textarea(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_string *attr = NULL;
	dom_exception err;

	err = dom_element_get_attribute(node,
			corestring_dom_rows, &attr);

	if (err == DOM_NO_ERR && attr != NULL) {
		if (parse_dimension(
				(const char *)dom_string_data(attr), false,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			hint->prop = CSS_PROP_HEIGHT;
			hint->data.length.unit = CSS_UNIT_EM;
			hint->status = CSS_HEIGHT_SET;
			css_hint_advance(&hint);
		}
		dom_string_unref(attr);
	}

	err = dom_element_get_attribute(node,
			corestring_dom_cols, &attr);

	if (err == DOM_NO_ERR && attr != NULL) {
		if (parse_dimension(
				(const char *)dom_string_data(attr), false,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			hint->prop = CSS_PROP_WIDTH;
			hint->data.length.unit = CSS_UNIT_EX;
			hint->status = CSS_WIDTH_SET;
			css_hint_advance(&hint);
		}
		dom_string_unref(attr);
	}
}

static void css_hint_height_width_canvas(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_string *attr = NULL;
	dom_exception err;
	bool set_dim = false;

	err = dom_element_get_attribute(node,
			corestring_dom_height, &attr);

	if (err == DOM_NO_ERR && attr != NULL) {
		if (parse_dimension(
				(const char *)dom_string_data(attr), true,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			hint->prop = CSS_PROP_HEIGHT;
			hint->data.length.unit = CSS_UNIT_PX;
			hint->status = CSS_HEIGHT_SET;
			css_hint_advance(&hint);
			set_dim = true;
		}
		dom_string_unref(attr);
	}
	if (set_dim == false) {
		/* canvas defaults to 150px tall */
		hint->prop = CSS_PROP_HEIGHT;
		hint->data.length.unit = CSS_UNIT_PX;
		hint->data.length.value = INTTOFIX(150);
		hint->status = CSS_HEIGHT_SET;
		css_hint_advance(&hint);
	} else {
		set_dim = false;
	}

	err = dom_element_get_attribute(node,
			corestring_dom_width, &attr);

	if (err == DOM_NO_ERR && attr != NULL) {
		if (parse_dimension(
				(const char *)dom_string_data(attr), true,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			hint->prop = CSS_PROP_WIDTH;
			hint->data.length.unit = CSS_UNIT_PX;
			hint->status = CSS_WIDTH_SET;
			css_hint_advance(&hint);
			set_dim = true;
		}
		dom_string_unref(attr);
	}
	if (set_dim == false) {
		/* canvas defaults to 300px wide */
		hint->prop = CSS_PROP_WIDTH;
		hint->data.length.unit = CSS_UNIT_PX;
		hint->data.length.value = INTTOFIX(300);
		hint->status = CSS_WIDTH_SET;
		css_hint_advance(&hint);
	}
}

static void css_hint_width_input(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &(hint_ctx.hints[hint_ctx.len]);
	dom_string *attr = NULL;
	dom_exception err;

	err = dom_element_get_attribute(node,
			corestring_dom_size, &attr);

	if (err == DOM_NO_ERR && attr != NULL) {
		if (parse_dimension(
				(const char *)dom_string_data(attr), false,
				&hint->data.length.value,
				&hint->data.length.unit)) {
			dom_string *attr2 = NULL;

			err = dom_element_get_attribute(node,
					corestring_dom_type, &attr2);
			if (err == DOM_NO_ERR) {

				hint->prop = CSS_PROP_WIDTH;
				hint->status = CSS_WIDTH_SET;

				if (attr2 == NULL ||
						dom_string_caseless_lwc_isequal(
						attr2,
						corestring_lwc_text) ||
						dom_string_caseless_lwc_isequal(
						attr2,
						corestring_lwc_search) ||
						dom_string_caseless_lwc_isequal(
						attr2,
						corestring_lwc_password) ||
						dom_string_caseless_lwc_isequal(
						attr2,
						corestring_lwc_file)) {
					hint->data.length.unit = CSS_UNIT_EX;
				}
				if (attr2 != NULL) {
					dom_string_unref(attr2);
				}
				css_hint_advance(&hint);
			}
		}
		dom_string_unref(attr);
	}
}

static void css_hint_anchor_color(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	css_error error;
	dom_exception err;
	dom_string *color;
	dom_node *bodynode = NULL;

	/* find body node */
	css_qname qs;
	bool is_visited;

	qs.ns = NULL;
	qs.name = lwc_string_ref(corestring_lwc_body);
	if (named_ancestor_node(ctx, node, &qs,
			(void *)&bodynode) != CSS_OK) {
		/* Didn't find, or had error */
		lwc_string_unref(qs.name);
		return ;
	}
	lwc_string_unref(qs.name);

	if (bodynode == NULL) {
		return;
	}

	error = node_is_visited(ctx, node, &is_visited);
	if (error != CSS_OK)
		return;

	if (is_visited) {
		err = dom_element_get_attribute(bodynode,
				corestring_dom_vlink, &color);
	} else {
		err = dom_element_get_attribute(bodynode,
				corestring_dom_link, &color);
	}

	if (err == DOM_NO_ERR && color != NULL) {
		if (nscss_parse_colour(
				(const char *)dom_string_data(color),
						&hint->data.color)) {
			hint->prop = CSS_PROP_COLOR;
			hint->status = CSS_COLOR_COLOR;
			css_hint_advance(&hint);
		}
		dom_string_unref(color);
	}
}

static void css_hint_body_color(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_exception err;
	dom_string *color;

	err = dom_element_get_attribute(node, corestring_dom_text, &color);

	if (err == DOM_NO_ERR && color != NULL) {
		if (nscss_parse_colour(
				(const char *)dom_string_data(color),
						&hint->data.color)) {
			hint->prop = CSS_PROP_COLOR;
			hint->status = CSS_COLOR_COLOR;
			css_hint_advance(&hint);
		}
		dom_string_unref(color);
	}
}

static void css_hint_color(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_exception err;
	dom_string *color;

	err = dom_element_get_attribute(node, corestring_dom_color, &color);

	if (err == DOM_NO_ERR && color != NULL) {
		if (nscss_parse_colour(
				(const char *)dom_string_data(color),
						&hint->data.color)) {
			hint->prop = CSS_PROP_COLOR;
			hint->status = CSS_COLOR_COLOR;
			css_hint_advance(&hint);
		}
		dom_string_unref(color);
	}
}

static void css_hint_font_size(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_exception err;
	dom_string *size;

	err = dom_element_get_attribute(node, corestring_dom_size, &size);
	if (err == DOM_NO_ERR && size != NULL) {
		if (parse_font_size(
				(const char *)dom_string_data(size),
						&hint->status,
						&hint->data.length.value,
						&hint->data.length.unit)) {
			hint->prop = CSS_PROP_FONT_SIZE;
			css_hint_advance(&hint);
		}
		dom_string_unref(size);
	}
}

static void css_hint_float(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_exception err;
	dom_string *align;

	err = dom_element_get_attribute(node, corestring_dom_align, &align);
	if (err == DOM_NO_ERR && align != NULL) {
		if (dom_string_caseless_lwc_isequal(align,
				corestring_lwc_left)) {
			hint->prop = CSS_PROP_FLOAT;
			hint->status = CSS_FLOAT_LEFT;
			css_hint_advance(&hint);

		} else if (dom_string_caseless_lwc_isequal(align,
				corestring_lwc_right)) {
			hint->prop = CSS_PROP_FLOAT;
			hint->status = CSS_FLOAT_RIGHT;
			css_hint_advance(&hint);
		}
		dom_string_unref(align);
	}
}

static void css_hint_caption_side(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_exception err;
	dom_string *align = NULL;

	err = dom_element_get_attribute(node, corestring_dom_align, &align);
	if (err == DOM_NO_ERR && align != NULL) {
		if (dom_string_caseless_lwc_isequal(align,
				corestring_lwc_bottom)) {
			hint->prop = CSS_PROP_CAPTION_SIDE;
			hint->status = CSS_CAPTION_SIDE_BOTTOM;
			css_hint_advance(&hint);
		}
		dom_string_unref(align);
	}
}

static void css_hint_bg_color(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &hint_ctx.hints[hint_ctx.len];
	dom_exception err;
	dom_string *bgcolor;

	err = dom_element_get_attribute(node,
			corestring_dom_bgcolor, &bgcolor);
	if (err == DOM_NO_ERR && bgcolor != NULL) {
		if (nscss_parse_colour(
				(const char *)dom_string_data(bgcolor),
				&hint->data.color)) {
			hint->prop = CSS_PROP_BACKGROUND_COLOR;
			hint->status = CSS_BACKGROUND_COLOR_COLOR;
			css_hint_advance(&hint);
		}
		dom_string_unref(bgcolor);
	}
}

static void css_hint_bg_image(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &(hint_ctx.hints[hint_ctx.len]);
	dom_exception err;
	dom_string *attr;

	err = dom_element_get_attribute(node,
			corestring_dom_background, &attr);
	if (err == DOM_NO_ERR && attr != NULL) {
		nsurl *url;
		nserror error = nsurl_join(ctx->base_url,
				(const char *)dom_string_data(attr), &url);
		dom_string_unref(attr);

		if (error == NSERROR_OK) {
			lwc_string *iurl;
			lwc_error lerror = lwc_intern_string(nsurl_access(url),
					nsurl_length(url), &iurl);
			nsurl_unref(url);

			if (lerror == lwc_error_ok) {
				hint->prop = CSS_PROP_BACKGROUND_IMAGE;
				hint->data.string = iurl;
				hint->status = CSS_BACKGROUND_IMAGE_IMAGE;
				css_hint_advance(&hint);
			}
		}
	}
}

static void css_hint_white_space_nowrap(
		nscss_select_ctx *ctx,
		dom_node *node)
{
	struct css_hint *hint = &(hint_ctx.hints[hint_ctx.len]);
	dom_exception err;
	bool nowrap;

	err = dom_element_has_attribute(node, corestring_dom_nowrap, &nowrap);
	if (err == DOM_NO_ERR && nowrap == true) {
		hint->prop = CSS_PROP_WHITE_SPACE;
		hint->status = CSS_WHITE_SPACE_NOWRAP;
		css_hint_advance(&hint);
	}
}


/* Exported function, documeted in css/hints.h */
css_error node_presentational_hint(void *pw, void *node,
		uint32_t *nhints, css_hint **hints)
{
	dom_exception exc;
	dom_html_element_type tag_type;

	css_hint_clean();

	exc = dom_html_element_get_tag_type(node, &tag_type);
	if (exc != DOM_NO_ERR) {
		tag_type = DOM_HTML_ELEMENT_TYPE__UNKNOWN;
	}

	switch (tag_type) {
	case DOM_HTML_ELEMENT_TYPE_TH:
	case DOM_HTML_ELEMENT_TYPE_TD:
		css_hint_width(pw, node);
		css_hint_table_cell_border_padding(pw, node);
		css_hint_white_space_nowrap(pw, node);
		/* fall through */
	case DOM_HTML_ELEMENT_TYPE_TR:
		css_hint_height(pw, node);
		/* fall through */
	case DOM_HTML_ELEMENT_TYPE_THEAD:
	case DOM_HTML_ELEMENT_TYPE_TBODY:
	case DOM_HTML_ELEMENT_TYPE_TFOOT:
		css_hint_text_align_special(pw, node);
		/* fall through */
	case DOM_HTML_ELEMENT_TYPE_COL:
		css_hint_vertical_align_table_cells(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_APPLET:
	case DOM_HTML_ELEMENT_TYPE_IMG:
		css_hint_margin_hspace_vspace(pw, node);
		/* fall through */
	case DOM_HTML_ELEMENT_TYPE_EMBED:
	case DOM_HTML_ELEMENT_TYPE_IFRAME:
	case DOM_HTML_ELEMENT_TYPE_OBJECT:
		css_hint_height(pw, node);
		css_hint_width(pw, node);
		css_hint_vertical_align_replaced(pw, node);
		css_hint_float(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_P:
	case DOM_HTML_ELEMENT_TYPE_H1:
	case DOM_HTML_ELEMENT_TYPE_H2:
	case DOM_HTML_ELEMENT_TYPE_H3:
	case DOM_HTML_ELEMENT_TYPE_H4:
	case DOM_HTML_ELEMENT_TYPE_H5:
	case DOM_HTML_ELEMENT_TYPE_H6:
		css_hint_text_align_normal(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_CENTER:
		css_hint_text_align_center(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_CAPTION:
		css_hint_caption_side(pw, node);
		/* fall through */
	case DOM_HTML_ELEMENT_TYPE_DIV:
		css_hint_text_align_special(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_TABLE:
		css_hint_text_align_table_special(pw, node);
		css_hint_table_spacing_border(pw, node);
		css_hint_float(pw, node);
		css_hint_margin_left_right_align_center(pw, node);
		css_hint_width(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_HR:
		css_hint_width(pw, node);
		css_hint_margin_left_right_hr(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_TEXTAREA:
		css_hint_height_width_textarea(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_INPUT:
		css_hint_width_input(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_A:
		css_hint_anchor_color(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_FONT:
		css_hint_font_size(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_BODY:
		css_hint_body_color(pw, node);
		break;
	case DOM_HTML_ELEMENT_TYPE_CANVAS:
		css_hint_height_width_canvas(pw, node);
		break;
	default:
		break;
	}

	if (tag_type != DOM_HTML_ELEMENT_TYPE__UNKNOWN) {
		css_hint_color(pw, node);
		css_hint_bg_color(pw, node);
		css_hint_bg_image(pw, node);
	}

#ifdef LOG_STATS
	NSLOG(netsurf, INFO, "Properties with hints: %i", hint_ctx.len);
#endif

	css_hint_get_hints(hints, nhints);

	return CSS_OK;
}
