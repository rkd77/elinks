/* Terminal kitty routines. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef CONFIG_GZIP
#include <zlib.h>
#endif

#include "elinks.h"

#include "document/document.h"
#include "osdep/osdep.h"
#include "terminal/hardio.h"
#include "terminal/kitty.h"
#include "terminal/map.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/base64.h"
#include "util/memory.h"

#define STB_IMAGE_IMPLEMENTATION
#include "terminal/stb_image.h"

unsigned char *
el_kitty_get_image(char *data, int length, int *outlen, int *width, int *height, int *compressed)
{
	ELOG
	int comp;
	unsigned char *pixels = stbi_load_from_memory((unsigned char *)data, length, width, height, &comp, KITTY_BYTES_PER_PIXEL);
	unsigned char *b64;

	if (!pixels) {
		return NULL;
	}
	int size = *width * *height * KITTY_BYTES_PER_PIXEL;
	*compressed = 0;

#ifdef CONFIG_GZIP
	unsigned char *complace = (unsigned char *)mem_alloc(size);

	if (complace) {
		unsigned long compsize = size;
		int res = compress(complace, &compsize, pixels, size);

		if (res == Z_OK) {
			*compressed = 1;
			b64 = base64_encode_bin(complace, compsize, outlen);
			stbi_image_free(pixels);
			mem_free(complace);
			return b64;
		}
		mem_free(complace);
	}
#endif
	b64 = base64_encode_bin(pixels, size, outlen);
	stbi_image_free(pixels);

	return b64;
}

int
add_kitty_image_to_document(struct document *doc, char *data, int datalen, int lineno, struct k_image **imagine, int width, int height)
{
	ELOG
	struct k_image *im = mem_calloc(1, sizeof(*im));

	if (imagine) {
		*imagine = NULL;
	}

	if (!im) {
		return 0;
	}
	im->pixels = (struct el_string *)mem_calloc(1, sizeof(struct el_string));

	if (!im->pixels) {
		mem_free(im);
		return 0;
	}
	im->cy = lineno;
	im->cx = 0;
	im->width = width;
	im->height = height;
	im->pixels->data = data;
	im->pixels->length = datalen;
	im->pixels->refcnt = 1;

	int ile = (height + doc->options.cell_height - 1) / doc->options.cell_height;
	add_to_list(doc->k_images, im);

	if (imagine) {
		*imagine = im;
	}

	return ile;
}

void
delete_k_image(struct k_image *im)
{
	ELOG
	del_from_list(im);

	if (--(im->pixels->refcnt) <= 0) {
		mem_free(im->pixels->data);
		mem_free(im->pixels);
	}
	mem_free(im);
}

void
try_to_draw_k_images(struct terminal *term)
{
	ELOG

	if (!term->kitty) {
		return;
	}
	struct string text;

	if (!init_string(&text)) {
		return;
	}
	add_to_string(&text, "\033_Ga=d\033\\");

	if (text.length) {
		if (term->master) want_draw();
		hard_write(term->fdout, text.source, text.length);
		if (term->master) done_draw();
	}
	done_string(&text);

	int i;

	for (i = 0; i < term->number_of_images; i++) {
		struct k_image *im = term->k_images[i];

		if (!init_string(&text)) {
			return;
		}
		add_cursor_move_to_string(&text, im->cy + 1, im->cx + 1);

		int id = get_id_from_ID(im->ID);

		if (id < 0) {
			int m;
			int left = im->pixels->length;
			int sent = 0;
			while (1) {
				m = left >= 4000;
				add_format_to_string(&text, "\033_Gf=%d,I=%d,s=%d,v=%d,m=%d,t=d,a=T%s;", KITTY_BYTES_PER_PIXEL * 8, im->ID, im->width, im->height, m, (im->compressed ? ",o=z": ""));
				add_bytes_to_string(&text, im->pixels->data + sent, m ? 4000 : left);
				add_to_string(&text, "\033\\");
				if (!m) {
					break;
				}
				sent += 4000;
				left -= 4000;
			};
		} else {
			add_format_to_string(&text, "\033_Gi=%d,x=%d,y=%d,w=%d,h=%d,a=p,q=1\033\\", id, im->x, im->y, im->w, im->h);
		}

		if (text.length) {
			if (term->master) want_draw();
			hard_write(term->fdout, text.source, text.length);
			if (term->master) done_draw();
		}
		done_string(&text);
	}
}

struct k_image *
copy_k_frame(struct k_image *src, struct el_box *box, int cell_width, int cell_height, int dx, int dy)
{
	ELOG
	struct k_image *dest = mem_calloc(1, sizeof(*dest));

	if (!dest) {
		return NULL;
	}
	dest->pixels = src->pixels;
	dest->pixels->refcnt++;

	int cx = src->cx - dx;
	int cy = src->cy - dy;

	int clipx = cx >= 0 ? 0 : (-cx * cell_width);
	int clipy = cy >= 0 ? 0 : (-cy * cell_height);

	int clipwidth = box->width * cell_width;
	int clipheight = box->height * cell_height;

	if (src->width < clipwidth) {
		clipwidth = src->width;
	}
	if (src->height < clipheight) {
		clipheight = src->height;
	}

	if (cx * cell_width + clipwidth >= box->width * cell_width) {
		clipwidth = (box->width * cell_width - cx * cell_width);
	}
	if (cy * cell_height + clipheight >= box->height * cell_height) {
		clipheight = (box->height * cell_height - cy * cell_height);
	}
	dest->x = clipx;
	dest->y = clipy;
	dest->w = clipwidth;
	dest->h = clipheight;

	dest->cx = cx < 0 ? 0 : cx;
	dest->cy = cy < 0 ? 0 : cy;
	dest->width = src->width;
	dest->height = src->height;

	dest->ID = src->ID;
	dest->number = src->number;
	dest->sent = src->sent;
	dest->compressed = src->compressed;

	return dest;
}
