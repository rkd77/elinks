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

unsigned int node_number_counter;

struct bfu_color_entry {
	/* Pointers to the options tree values. */
	color_T *background;
	color_T *foreground;

	/* The copy of "text" and "background" colors. */
	struct color_pair colors;

	unsigned int node_number;

	unsigned int was_mono:1;
	unsigned int was_color16:1;
	unsigned int was_color256:1;
	unsigned int was_color24:1;

	struct screen_char mono;
	struct screen_char c16;
	struct screen_char c256;
	struct screen_char c24;
};

struct bfu_color_entry *node_entries[1024];

static struct hash *bfu_colors = NULL;

struct screen_char *
get_bfu_mono_node(unsigned int node_number)
{
	struct bfu_color_entry *entry = node_entries[node_number];

	if (!entry->was_mono) {
		set_term_color(&entry->mono, &entry->colors, 0, COLOR_MODE_MONO);
		entry->was_mono = 1;
	}

	return &entry->mono;
}

unsigned char *
get_bfu_background_color16_node(unsigned int node_number)
{
	struct bfu_color_entry *entry = node_entries[node_number];

	if (!entry->was_color16) {
		set_term_color(&entry->c16, &entry->colors, 0, COLOR_MODE_16);
		entry->was_color16 = 1;
	}

	return &entry->c16.c.color[0];
}

unsigned char *
get_bfu_foreground_color16_node(unsigned int node_number)
{
	struct bfu_color_entry *entry = node_entries[node_number];

	if (!entry->was_color16) {
		set_term_color(&entry->c16, &entry->colors, 0, COLOR_MODE_16);
		entry->was_color16 = 1;
	}

	return &entry->c16.c.color[0];
}

unsigned char
get_bfu_background_color256_node(unsigned int node_number)
{
	struct bfu_color_entry *entry = node_entries[node_number];

	if (!entry->was_color256) {
		set_term_color(&entry->c256, &entry->colors, 0, COLOR_MODE_256);
		entry->was_color256 = 1;
	}

	return TERM_COLOR_BACKGROUND_256(entry->c256.c.color);
}

unsigned char
get_bfu_foreground_color256_node(unsigned int node_number)
{
	struct bfu_color_entry *entry = node_entries[node_number];

	if (!entry->was_color256) {
		set_term_color(&entry->c256, &entry->colors, 0, COLOR_MODE_256);
		entry->was_color256 = 1;
	}

	return TERM_COLOR_FOREGROUND_256(entry->c256.c.color);
}


unsigned char *
get_bfu_background_color_true_node(unsigned int node_number)
{
	struct bfu_color_entry *entry = node_entries[node_number];

	if (!entry->was_color24) {
		set_term_color(&entry->c24, &entry->colors, 0, COLOR_MODE_TRUE_COLOR);
		entry->was_color24 = 1;
	}

	return &entry->c24.c.color[3];
}

unsigned char *
get_bfu_foreground_color_true_node(unsigned int node_number)
{
	struct bfu_color_entry *entry = node_entries[node_number];

	if (!entry->was_color24) {
		set_term_color(&entry->c24, &entry->colors, 0, COLOR_MODE_TRUE_COLOR);
		entry->was_color24 = 1;
	}

	return &entry->c24.c.color[0];
}


static struct bfu_color_entry *
get_bfu_color_common(struct terminal *term, const char *stylename)
{
	static color_mode_T last_color_mode;
	struct bfu_color_entry *entry;
	int stylenamelen;
	struct hash_item *item;
	color_mode_T color_mode;

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
	entry = (struct bfu_color_entry *)(item ? item->value : NULL);

	if (!entry) {
		struct option *opt;

		/* Construct the color entry. */
		opt = get_opt_rec_real(config_options,
				       color_mode != COLOR_MODE_MONO
				       ? "ui.colors.color" : "ui.colors.mono");
		if (!opt) return NULL;

		opt = get_opt_rec_real(opt, stylename);
		if (!opt) return NULL;

		entry = (struct bfu_color_entry *)mem_calloc(1, sizeof(*entry));
		if (!entry) return NULL;

		item = add_hash_item(bfu_colors, stylename, stylenamelen, entry);
		if (!item) {
			mem_free(entry);
			return NULL;
		}

		entry->foreground = &get_opt_color_tree(opt, "text", NULL);
		entry->background = &get_opt_color_tree(opt, "background", NULL);

		entry->node_number = ++node_number_counter;
		node_entries[node_number_counter] = entry;
	}

	/* Always update the color pair. */
	entry->colors.background = *entry->background;
	entry->colors.foreground = *entry->foreground;

	return entry;
}

struct color_pair *
get_bfu_color(struct terminal *term, const char *stylename)
{
	struct bfu_color_entry *entry = get_bfu_color_common(term, stylename);

	if (!entry) {
		return NULL;
	}

	return &entry->colors;
}

unsigned int
get_bfu_color_node(struct terminal *term, const char *stylename)
{
	struct bfu_color_entry *entry = get_bfu_color_common(term, stylename);

	if (!entry) {
		return 0;
	}

	return entry->node_number;
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
