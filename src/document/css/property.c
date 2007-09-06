/** CSS property info
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "document/css/property.h"
#include "document/css/value.h"


/** @todo TODO: Use fastfind when we get a lot of properties.
 * XXX: But only _WHEN_ we get _A LOT_ of properties, zas! ;-) --pasky */
struct css_property_info css_property_info[CSS_PT_LAST] = {
	{ "font-size",          CSS_PT_FONT_SIZE,        CSS_VT_LENGTH,		css_parse_length_value },
	{ "background",		CSS_PT_BACKGROUND,	 CSS_VT_COLOR,		css_parse_background_value },
	{ "background-color",	CSS_PT_BACKGROUND_COLOR, CSS_VT_COLOR,		css_parse_color_value },
	{ "color",		CSS_PT_COLOR,		 CSS_VT_COLOR,		css_parse_color_value },
	{ "display",		CSS_PT_DISPLAY,		 CSS_VT_DISPLAY,	css_parse_display_value },
	{ "font-style",		CSS_PT_FONT_STYLE,	 CSS_VT_FONT_ATTRIBUTE,	css_parse_font_style_value },
	{ "font-weight",	CSS_PT_FONT_WEIGHT,	 CSS_VT_FONT_ATTRIBUTE,	css_parse_font_weight_value },
	{ "margin-left",	CSS_PT_MARGIN_LEFT,	 CSS_VT_LENGTH,		css_parse_length_value },
	{ "margin-right",	CSS_PT_MARGIN_RIGHT,	 CSS_VT_LENGTH,		css_parse_length_value },
	{ "text-align",		CSS_PT_TEXT_ALIGN,	 CSS_VT_TEXT_ALIGN,	css_parse_text_align_value },
	{ "text-decoration",	CSS_PT_TEXT_DECORATION,	 CSS_VT_FONT_ATTRIBUTE,	css_parse_text_decoration_value },
	{ "white-space",	CSS_PT_WHITE_SPACE,	 CSS_VT_FONT_ATTRIBUTE,	css_parse_white_space_value },

	{ NULL, CSS_PT_NONE, CSS_VT_NONE },
};
