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
init_document_options(struct session *ses, struct document_options *doo)
{
	/* Ensure that any padding bytes are cleared. */
	memset(doo, 0, sizeof(*doo));

	doo->assume_cp = get_opt_codepage("document.codepage.assume", NULL);
	doo->hard_assume = get_opt_bool("document.codepage.force_assumed", NULL);

	doo->use_document_colors = get_opt_int("document.colors.use_document_colors", ses);
	doo->margin = get_opt_int("document.browse.margin_width", NULL);
	doo->num_links_key = get_opt_int("document.browse.links.number_keys_select_link", NULL);
	doo->meta_link_display = get_opt_int("document.html.link_display", NULL);
	doo->default_form_input_size = get_opt_int("document.browse.forms.input_size", NULL);

	/* Color options. */
	doo->default_fg = get_opt_color("document.colors.text", NULL);
	doo->default_bg = get_opt_color("document.colors.background", NULL);
	doo->default_link = get_opt_color("document.colors.link", NULL);
	doo->default_vlink = get_opt_color("document.colors.vlink", NULL);
#ifdef CONFIG_BOOKMARKS
	doo->default_bookmark_link = get_opt_color("document.colors.bookmark", NULL);
#endif
	doo->default_image_link = get_opt_color("document.colors.image", NULL);

	doo->active_link.fg = get_opt_color("document.browse.links.active_link.colors.text", NULL);
	doo->active_link.bg = get_opt_color("document.browse.links.active_link.colors.background", NULL);

	if (get_opt_bool("document.colors.increase_contrast", NULL))
		doo->color_flags |= COLOR_INCREASE_CONTRAST;

	if (get_opt_bool("document.colors.ensure_contrast", NULL))
		doo->color_flags |= COLOR_ENSURE_CONTRAST;

	/* Boolean options. */
#ifdef CONFIG_CSS
	doo->css_enable = get_opt_bool("document.css.enable", ses);
	doo->css_import = get_opt_bool("document.css.import", NULL);
#endif

	doo->plain_display_links = get_opt_bool("document.plain.display_links", NULL);
	doo->plain_compress_empty_lines = get_opt_bool("document.plain.compress_empty_lines", ses);
	doo->underline_links = get_opt_bool("document.html.underline_links", NULL);
	doo->wrap_nbsp = get_opt_bool("document.html.wrap_nbsp", NULL);
	doo->use_tabindex = get_opt_bool("document.browse.links.use_tabindex", NULL);
	doo->links_numbering = get_opt_bool("document.browse.links.numbering", ses);

	doo->active_link.color = get_opt_bool("document.browse.links.active_link.enable_color", NULL);
	doo->active_link.invert = get_opt_bool("document.browse.links.active_link.invert", NULL);
	doo->active_link.underline = get_opt_bool("document.browse.links.active_link.underline", NULL);
	doo->active_link.bold = get_opt_bool("document.browse.links.active_link.bold", NULL);

	doo->table_order = get_opt_bool("document.browse.table_move_order", NULL);
	doo->tables = get_opt_bool("document.html.display_tables", ses);
	doo->frames = get_opt_bool("document.html.display_frames", NULL);
	doo->images = get_opt_bool("document.browse.images.show_as_links", ses);
	doo->display_subs = get_opt_bool("document.html.display_subs", NULL);
	doo->display_sups = get_opt_bool("document.html.display_sups", NULL);

	doo->framename = "";

	doo->image_link.prefix = "";
	doo->image_link.suffix = "";
	doo->image_link.filename_maxlen = get_opt_int("document.browse.images.filename_maxlen", NULL);
	doo->image_link.label_maxlen = get_opt_int("document.browse.images.label_maxlen", NULL);
	doo->image_link.display_style = get_opt_int("document.browse.images.display_style", NULL);
	doo->image_link.tagging = get_opt_int("document.browse.images.image_link_tagging", NULL);
	doo->image_link.show_any_as_links = get_opt_bool("document.browse.images.show_any_as_links", NULL);
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
	o1->image_link.prefix = stracpy(get_opt_str("document.browse.images.image_link_prefix", NULL));
	o1->image_link.suffix = stracpy(get_opt_str("document.browse.images.image_link_suffix", NULL));
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
	assert(option);
	if_assert_failed return;

	if (ses->option)
		option = get_option_shadow(option, config_options, ses->option);
	if (!option) return;

	toggle_option(ses, option);

	draw_formatted(ses, 1);
}
