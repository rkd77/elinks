/** Terminal sixel routines.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>

#include "elinks.h"

#include "document/document.h"
#include "osdep/osdep.h"
#include "terminal/hardio.h"
#include "terminal/screen.h"
#include "terminal/sixel.h"
#include "terminal/terminal.h"

/* encode settings object */
struct sixel_decoder {
	unsigned int ref;
	char *input;
	char *output;
	sixel_allocator_t *allocator;
};

void
try_to_draw_images(struct terminal *term)
{
	struct image *im;

	foreach (im, term->images) {
		struct string text;

		if (!init_string(&text)) {
			return;
		}
		add_cursor_move_to_string(&text, im->y + 1, im->x + 1);
		add_string_to_string(&text, &im->sixel);

		if (text.length) {
			if (term->master) want_draw();
			hard_write(term->fdout, text.source, text.length);
			if (term->master) done_draw();
		}
		done_string(&text);
	}
}

void
delete_image(struct image *im)
{
	del_from_list(im);
	done_string(&im->sixel);
	mem_free(im);
}

int
add_image_to_document(struct document *doc, struct string *pixels, int lineno)
{
	struct image *im = mem_calloc(1, sizeof(*im));

	if (!im) {
		return 0;
	}
	sixel_decoder_t *decoder = NULL;
	SIXELSTATUS status = sixel_decoder_new(&decoder, NULL);

	if (status != SIXEL_OK) {
		return 0;
	}

	unsigned char *indexed_pixels = NULL;
	unsigned char *palette = NULL;
	int ncolors;
	int width;
	int height;
	int ile = 0;

	status = sixel_decode_raw(
		(unsigned char *)pixels->source,
		pixels->length,
		&indexed_pixels,
		&width,
		&height,
		&palette,
		&ncolors,
		decoder->allocator
	);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	im->y = lineno + 1;
	im->x = 0;
	im->width = width;
	im->height = height;
	*(&im->sixel) = *pixels;
	ile = (height + 15) / 16;
	add_to_list(doc->images, im);
end:
	sixel_allocator_free(decoder->allocator, indexed_pixels);
	sixel_allocator_free(decoder->allocator, palette);
	sixel_decoder_unref(decoder);

	return ile;
}
