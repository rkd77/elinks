/** BFU style/color cache
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/style.h"
#include "config/options.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/hash.h"


struct bfu_color_entry {
	/* Pointers to the options tree values. */
	color_T *background;
	color_T *foreground;

	/* The copy of "text" and "background" colors. */
	struct color_pair colors;
};

static struct hash *bfu_colors = NULL;

struct color_pair *
get_bfu_color(struct terminal *term, unsigned char *stylename)
{
	static enum color_mode last_color_mode;
	struct bfu_color_entry *entry;
	int stylenamelen;
	struct hash_item *item;
	enum color_mode color_mode;

	if (!term) return NULL;

	color_mode = get_opt_int_tree(term->spec, "colors", NULL);

	if (!bfu_colors) {
		/* Initialize the style hash. */
		bfu_colors = init_hash8();
		if (!bfu_colors) return NULL;

		last_color_mode = color_mode;

	} else if (color_mode != last_color_mode) {
		int i;

		/* Change mode by emptying the cache so mono/color colors
		 * aren't mixed. */
		foreach_hash_item (item, *bfu_colors, i) {
			mem_free_if(item->value);
			item = item->prev;
			del_hash_item(bfu_colors, item->next);
		}

		last_color_mode = color_mode;
	}

	stylenamelen = strlen(stylename);
	item = get_hash_item(bfu_colors, stylename, stylenamelen);
	entry = item ? item->value : NULL;

	if (!entry) {
		struct option *opt;

		/* Construct the color entry. */
		opt = get_opt_rec_real(config_options,
				       color_mode != COLOR_MODE_MONO
				       ? "ui.colors.color" : "ui.colors.mono");
		if (!opt) return NULL;

		opt = get_opt_rec_real(opt, stylename);
		if (!opt) return NULL;

		entry = mem_calloc(1, sizeof(*entry));
		if (!entry) return NULL;

		item = add_hash_item(bfu_colors, stylename, stylenamelen, entry);
		if (!item) {
			mem_free(entry);
			return NULL;
		}

		entry->foreground = &get_opt_color_tree(opt, "text", NULL);
		entry->background = &get_opt_color_tree(opt, "background", NULL);
	}

	/* Always update the color pair. */
	entry->colors.background = *entry->background;
	entry->colors.foreground = *entry->foreground;

	return &entry->colors;
}

void
done_bfu_colors(void)
{
	struct hash_item *item;
	int i;

	if (!bfu_colors)
		return;

	foreach_hash_item (item, *bfu_colors, i) {
		mem_free_if(item->value);
	}

	free_hash(&bfu_colors);
};
