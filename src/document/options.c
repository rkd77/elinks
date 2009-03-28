/** Document options/setup workshop
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stddef.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "dialogs/document.h"
#include "document/options.h"
#include "document/view.h"
#include "session/session.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/string.h"
#include "viewer/text/draw.h"


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
	doo->default_style.fg = get_opt_color("document.colors.text");
	doo->default_style.bg = get_opt_color("document.colors.background");
	doo->default_link = get_opt_color("document.colors.link");
	doo->default_vlink = get_opt_color("document.colors.vlink");
#ifdef CONFIG_BOOKMARKS
	doo->default_bookmark_link = get_opt_color("document.colors.bookmark");
#endif
	doo->default_image_link = get_opt_color("document.colors.image");

	doo->active_link.fg = get_opt_color("document.browse.links.active_link.colors.text");
	doo->active_link.bg = get_opt_color("document.browse.links.active_link.colors.background");

	if (get_opt_bool("document.colors.increase_contrast"))
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
	doo->links_numbering = get_opt_bool("document.browse.links.numbering");

	doo->active_link.color = get_opt_bool("document.browse.links.active_link.enable_color");
	doo->active_link.invert = get_opt_bool("document.browse.links.active_link.invert");
	doo->active_link.underline = get_opt_bool("document.browse.links.active_link.underline");
	doo->active_link.bold = get_opt_bool("document.browse.links.active_link.bold");

	doo->table_order = get_opt_bool("document.browse.table_move_order");
	doo->tables = get_opt_bool("document.html.display_tables");
	doo->frames = get_opt_bool("document.html.display_frames");
	doo->images = get_opt_bool("document.browse.images.show_as_links");
	doo->display_subs = get_opt_bool("document.html.display_subs");
	doo->display_sups = get_opt_bool("document.html.display_sups");

	doo->framename = "";

	doo->image_link.prefix = "";
	doo->image_link.suffix = "";
	doo->image_link.filename_maxlen = get_opt_int("document.browse.images.filename_maxlen");
	doo->image_link.label_maxlen = get_opt_int("document.browse.images.label_maxlen");
	doo->image_link.display_style = get_opt_int("document.browse.images.display_style");
	doo->image_link.tagging = get_opt_int("document.browse.images.image_link_tagging");
	doo->image_link.show_any_as_links = get_opt_bool("document.browse.images.show_any_as_links");
}

int
compare_opt(struct document_options *o1, struct document_options *o2)
{
	return memcmp(o1, o2, offsetof(struct document_options, framename))
		|| c_strcasecmp(o1->framename, o2->framename)
		|| (o1->box.x != o2->box.x)
		|| (o1->box.y != o2->box.y)
		|| ((o1->needs_height || o2->needs_height)
		    && o1->box.height != o2->box.height)
		|| ((o1->needs_width || o2->needs_width)
		    && o1->box.width != o2->box.width);
}

NONSTATIC_INLINE void
copy_opt(struct document_options *o1, struct document_options *o2)
{
	copy_struct(o1, o2);
	o1->framename = stracpy(o2->framename);
	o1->image_link.prefix = stracpy(get_opt_str("document.browse.images.image_link_prefix"));
	o1->image_link.suffix = stracpy(get_opt_str("document.browse.images.image_link_suffix"));
}

void
done_document_options(struct document_options *options)
{
	mem_free_if(options->framename);
	mem_free(options->image_link.prefix);
	mem_free(options->image_link.suffix);
}

void
toggle_document_option(struct session *ses, unsigned char *option_name)
{
	struct option *option;

	assert(ses && ses->doc_view && ses->tab && ses->tab->term);
	if_assert_failed return;

	if (!ses->doc_view->vs) {
		nowhere_box(ses->tab->term, NULL);
		return;
	}

	option = get_opt_rec(config_options, option_name);

	/* TODO: toggle per document. --Zas */
	toggle_option(ses, option);

	draw_formatted(ses, 1);
}
