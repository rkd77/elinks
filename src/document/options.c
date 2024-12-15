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

	doo->assume_cp = get_opt_codepage("document.codepage.assume", ses);
	doo->hard_assume = get_opt_bool("document.codepage.force_assumed", ses);

	doo->use_document_colors = get_opt_int("document.colors.use_document_colors", ses);
	doo->margin = get_opt_int("document.browse.margin_width", ses);

	doo->document_width = 0;
	if (get_opt_bool("document.browse.use_preferred_document_width", ses))
		doo->document_width = get_opt_int("document.browse.preferred_document_width", ses);

	if (ses) {
		if (doo->document_width <= 0 || doo->document_width > ses->tab->term->width)
			doo->document_width = ses->tab->term->width;
	} else {
		/* Assume we are in -dump mode.  Should we consolidate
		 * document.dump.width with document.browse.preferred_document_width ? */
		doo->document_width = get_opt_int("document.dump.width", NULL);
	}

	doo->num_links_key = get_opt_int("document.browse.links.number_keys_select_link", ses);
	doo->meta_link_display = get_opt_int("document.html.link_display", ses);
	doo->default_form_input_size = get_opt_int("document.browse.forms.input_size", ses);

	/* Color options. */
	doo->default_style.color.foreground = get_opt_color("document.colors.text", ses);
	doo->default_style.color.background = get_opt_color("document.colors.background", ses);
	doo->default_color.link = get_opt_color("document.colors.link", ses);
	doo->default_color.vlink = get_opt_color("document.colors.vlink", ses);
#ifdef CONFIG_BOOKMARKS
	doo->default_color.bookmark_link = get_opt_color("document.colors.bookmark", ses);
#endif
	doo->default_color.image_link = get_opt_color("document.colors.image", ses);
	doo->default_color.link_number = get_opt_color("document.colors.link_number", ses);
	doo->use_link_number_color = get_opt_bool("document.colors.use_link_number_color", ses);

	doo->active_link.color.foreground = get_opt_color("document.browse.links.active_link.colors.text", ses);
	doo->active_link.color.background = get_opt_color("document.browse.links.active_link.colors.background", ses);
	doo->active_link.insert_mode_color.foreground = get_opt_color("document.browse.links.active_link.insert_mode_colors.text", ses);
	doo->active_link.insert_mode_color.background = get_opt_color("document.browse.links.active_link.insert_mode_colors.background", ses);

	if (get_opt_bool("document.colors.increase_contrast", ses))
		doo->color_flags |= COLOR_INCREASE_CONTRAST;

	if (get_opt_bool("document.colors.ensure_contrast", ses))
		doo->color_flags |= COLOR_ENSURE_CONTRAST;

	/* Boolean options. */
#ifdef CONFIG_CSS
#ifdef CONFIG_LIBCSS
	doo->libcss_enable = get_opt_bool("document.css.libcss", ses);
#endif
	doo->css_enable = get_opt_bool("document.css.enable", ses);
	doo->css_ignore_display_none = get_opt_bool("document.css.ignore_display_none", ses);
	doo->css_import = get_opt_bool("document.css.import", ses);
#endif

	doo->plain_display_links = get_opt_bool("document.plain.display_links", ses);
	doo->plain_compress_empty_lines = get_opt_bool("document.plain.compress_empty_lines", ses);
	doo->plain_fixup_tables = get_opt_bool("document.plain.fixup_tables", ses);
	doo->underline_links = get_opt_bool("document.html.underline_links", ses);
	doo->html_subst_frame_chars = get_opt_bool("document.html.subst_frame_chars", ses);

	doo->wrap_nbsp = get_opt_bool("document.html.wrap_nbsp", ses);
	doo->use_tabindex = get_opt_bool("document.browse.links.use_tabindex", ses);
	doo->links_numbering = get_opt_bool("document.browse.links.numbering", ses);
	doo->links_show_goto = get_opt_bool("document.browse.links.show_goto", ses);

	doo->active_link.enable_color = get_opt_bool("document.browse.links.active_link.enable_color", ses);
	doo->active_link.invert = get_opt_bool("document.browse.links.active_link.invert", ses);
	doo->active_link.underline = get_opt_bool("document.browse.links.active_link.underline", ses);
	doo->active_link.bold = get_opt_bool("document.browse.links.active_link.bold", ses);

	doo->table_order = get_opt_bool("document.browse.table_move_order", ses);
	doo->tables = get_opt_bool("document.html.display_tables", ses);
	doo->frames = get_opt_bool("document.html.display_frames", ses);
	doo->iframes = get_opt_bool("document.html.display_iframes", ses);
	doo->images = get_opt_bool("document.browse.images.show_as_links", ses);
	doo->display_subs = get_opt_bool("document.html.display_subs", ses);
	doo->display_sups = get_opt_bool("document.html.display_sups", ses);

	doo->framename = (char *)"";

	doo->image_link.prefix = (char *)"";
	doo->image_link.suffix = (char *)"";
	doo->image_link.filename_maxlen = get_opt_int("document.browse.images.filename_maxlen", ses);
	doo->image_link.label_maxlen = get_opt_int("document.browse.images.label_maxlen", ses);
	doo->image_link.display_style = get_opt_int("document.browse.images.display_style", ses);
	doo->image_link.tagging = get_opt_int("document.browse.images.image_link_tagging", ses);
	doo->image_link.show_any_as_links = get_opt_bool("document.browse.images.show_any_as_links", ses);
#ifdef CONFIG_LIBSIXEL
	doo->sixel = get_opt_bool("document.plain.sixel", ses);
	if (ses && ses->tab->term) {
		doo->cell_width = ses->tab->term->cell_width;
		doo->cell_height = ses->tab->term->cell_height;
	}
#endif

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
toggle_document_option(struct session *ses, const char *option_name)
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
