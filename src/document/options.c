/* Document options/setup workshop */
/* $Id: options.c,v 1.57.2.2 2005/04/05 21:08:40 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "document/options.h"
#include "util/color.h"
#include "util/string.h"


struct document_options *global_doc_opts;

void
init_document_options(struct document_options *doo)
{
	/* Ensure that any padding bytes are cleared. */
	memset(doo, 0, sizeof(*doo));

	doo->assume_cp = get_opt_codepage("document.codepage.assume");
	doo->hard_assume = get_opt_bool("document.codepage.force_assumed");

	doo->use_document_colors = get_opt_int("document.colors.use_document_colors");
	doo->margin = get_opt_int("document.browse.margin_width");
	doo->num_links_key = get_opt_int("document.browse.links.number_keys_select_link");
	doo->meta_link_display = get_opt_int("document.html.link_display");
	doo->default_form_input_size = get_opt_int("document.browse.forms.input_size");

	/* Color options. */
	doo->default_fg = get_opt_color("document.colors.text");
	doo->default_bg = get_opt_color("document.colors.background");
	doo->default_link = get_opt_color("document.colors.link");
	doo->default_vlink = get_opt_color("document.colors.vlink");
#ifdef CONFIG_BOOKMARKS
	doo->default_bookmark_link = get_opt_color("document.colors.bookmark");
#endif
	doo->default_image_link = get_opt_color("document.colors.image");

	doo->active_link_fg = get_opt_color("document.browse.links.active_link.colors.text");
	doo->active_link_bg = get_opt_color("document.browse.links.active_link.colors.background");

	if (!get_opt_bool("document.colors.allow_dark_on_black"))
		doo->color_flags |= COLOR_INCREASE_CONTRAST;

	if (get_opt_bool("document.colors.ensure_contrast"))
		doo->color_flags |= COLOR_ENSURE_CONTRAST;

	/* Boolean options. */
#ifdef CONFIG_CSS
	doo->css_enable = get_opt_bool("document.css.enable");
	doo->css_import = get_opt_bool("document.css.import");
#endif

	doo->plain_display_links = get_opt_bool("document.plain.display_links");
	doo->plain_compress_empty_lines = get_opt_bool("document.plain.compress_empty_lines");
	doo->underline_links = get_opt_bool("document.html.underline_links");
	doo->wrap_nbsp = get_opt_bool("document.html.wrap_nbsp");
	doo->use_tabindex = get_opt_bool("document.browse.links.use_tabindex");
	doo->num_links_display = get_opt_bool("document.browse.links.numbering");

	doo->color_active_link = get_opt_bool("document.browse.links.active_link.enable_color");
	doo->invert_active_link = get_opt_bool("document.browse.links.active_link.invert");
	doo->underline_active_link = get_opt_bool("document.browse.links.active_link.underline");
	doo->bold_active_link = get_opt_bool("document.browse.links.active_link.bold");

	doo->table_order = get_opt_bool("document.browse.table_move_order");
	doo->tables = get_opt_bool("document.html.display_tables");
	doo->frames = get_opt_bool("document.html.display_frames");
	doo->images = get_opt_bool("document.browse.images.show_as_links");
	doo->display_subs = get_opt_bool("document.html.display_subs");
	doo->display_sups = get_opt_bool("document.html.display_sups");

	doo->framename = "";
}

int
compare_opt(struct document_options *o1, struct document_options *o2)
{
	return memcmp(o1, o2, offsetof(struct document_options, framename))
		|| strcasecmp(o1->framename, o2->framename)
		|| (o1->box.x != o2->box.x)
		|| (o1->box.y != o2->box.y)
		|| ((o1->needs_height || o2->needs_height)
		    && o1->box.height != o2->box.height)
		|| ((o1->needs_width || o2->needs_width)
		    && o1->box.width != o2->box.width);
}

inline void
copy_opt(struct document_options *o1, struct document_options *o2)
{
	copy_struct(o1, o2);
	o1->framename = stracpy(o2->framename);
}
