/* $Id: options.h,v 1.54 2004/12/20 11:22:11 miciah Exp $ */

#ifndef EL__DOCUMENT_OPTIONS_H
#define EL__DOCUMENT_OPTIONS_H

#include "terminal/color.h"
#include "util/color.h"
#include "util/box.h"

/* This mostly acts as a option cache so rendering will be faster. However it
 * is also used to validate and invalidate documents in the format cache as to
 * whether they satisfy the current state of the document options. */
struct document_options {
	enum color_mode color_mode;
	int cp, assume_cp, hard_assume;
	int margin;
	int num_links_key;
	int use_document_colors;
	int meta_link_display;
	int default_form_input_size;

	/* The default (fallback) colors. */
	color_t default_fg;
	color_t default_bg;
	color_t default_link;
	color_t default_vlink;
#ifdef CONFIG_BOOKMARKS
	color_t default_bookmark_link;
#endif
	color_t default_image_link;

	/* Color model/optimizations */
	enum color_flags color_flags;

	/* XXX: Keep boolean options grouped to save padding */
#ifdef CONFIG_CSS
	/* CSS stuff */
	unsigned int css_enable:1;
	unsigned int css_import:1;
#endif

	/* HTML stuff */
	unsigned int tables:1;
	unsigned int table_order:1;
	unsigned int frames:1;
	unsigned int images:1;

	unsigned int display_subs:1;
	unsigned int display_sups:1;
	unsigned int underline_links:1;

	unsigned int wrap_nbsp:1;

	/* Plain rendering stuff */
	unsigned int plain_display_links:1;
	unsigned int plain_compress_empty_lines:1;

	/* Link navigation */
	unsigned int num_links_display:1;
	unsigned int use_tabindex:1;

	unsigned int plain:1;
	unsigned int wrap:1;

	/* XXX: Everything past this comment is specialy handled by compare_opt() */
	unsigned char *framename;

	/* The position of the window (box.x and box.y)
	 *
	 *	This is not compared at all since it doesn't make any
	 *	difference what position the document will fit into a frameset
	 *	or so.
	 *
	 * The width of the window (box.width)
	 *
	 *	This controls how wide tables can be rendered and so on. It is
	 *	thus also to blame for the extra memory consumption when
	 *	resizing because all documents has to be rerendered. We only
	 *	need to compare it if not @plain.
	 *
	 * The height of the window (box.height)
	 *
	 *	Only documents containing textarea or frames uses it and we
	 *	only compare it if @needs_height is set.
	 */
	struct box box;
	unsigned int needs_height:1;
	unsigned int needs_width:1;

	/* Internal flag for rerendering */
	unsigned int no_cache:1;
	unsigned int gradual_rerendering:1;

	/* Active link coloring */
	/* This is mostly here to make use of this option cache so link
	 * drawing is faster. --jonas */
	unsigned int color_active_link:1;
	unsigned int underline_active_link:1;
	unsigned int bold_active_link:1;
	unsigned int invert_active_link:1;
	color_t active_link_fg;
	color_t active_link_bg;
};

extern struct document_options *global_doc_opts;

/* Fills the structure with values from the option system. */
void init_document_options(struct document_options *doo);

/* Copies the values of one struct @from to the other @to.
 * Note that the framename is dynamically allocated. */
void copy_opt(struct document_options *to, struct document_options *from);

/* Compares comparable values from the two structures according to
 * the comparable members described in the struct definition. */
int compare_opt(struct document_options *o1, struct document_options *o2);

#define use_document_fg_colors(o) \
	((o)->color_mode != COLOR_MODE_MONO && (o)->use_document_colors >= 1)

#define use_document_bg_colors(o) \
	((o)->color_mode != COLOR_MODE_MONO && (o)->use_document_colors == 2)

#endif
