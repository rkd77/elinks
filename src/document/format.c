/** Format attributes utilities
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/format.h"
#include "document/options.h"
#include "terminal/draw.h"
#include "util/color.h"


void
get_screen_char_template(struct screen_char *template_,
			 struct document_options *options,
			 struct text_style style)
{
	template_->attr = 0;
	template_->data = ' ';

	if (style.attr) {
		if (style.attr & AT_UNDERLINE) {
			template_->attr |= SCREEN_ATTR_UNDERLINE;
		}

		if (style.attr & AT_BOLD) {
			template_->attr |= SCREEN_ATTR_BOLD;
		}

		if (style.attr & AT_ITALIC) {
			template_->attr |= SCREEN_ATTR_ITALIC;
		}

		if (style.attr & AT_GRAPHICS) {
			template_->attr |= SCREEN_ATTR_FRAME;
		}
	}

	{
		struct color_pair colors = INIT_COLOR_PAIR(style.color.background, style.color.foreground);
		set_term_color(template_, &colors, options->color_flags, options->color_mode);
	}
}
