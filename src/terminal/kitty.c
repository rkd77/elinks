/* Terminal kitty routines. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

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

int
add_kitty_image_to_document(struct document *doc, struct el_string *pixels, int lineno, struct k_image **imagine, int width, int height)
{
	ELOG
	struct k_image *im = mem_calloc(1, sizeof(*im));

	if (imagine) {
		*imagine = NULL;
	}

	if (!im) {
		return 0;
	}
	im->pixels = el_string_ref(pixels);
	im->cy = lineno;
	im->cx = 0;
	im->width = width;
	im->height = height;

	int ile = (height + doc->options.cell_height - 1) / doc->options.cell_height;
	add_to_list(doc->k_images, im);

	if (imagine) {
		*imagine = im;
	}

	return ile;
}

struct el_string *
el_string_ref(struct el_string *el_string)
{
	if (el_string) {
		el_string->refcnt++;
	}

	return el_string;
}

void
el_string_unref(struct el_string *el_string)
{
	if (el_string) {
		if (--(el_string->refcnt) <= 0) {
			mem_free(el_string->data);
			mem_free(el_string);
		}
	}
}

void
delete_k_image(struct k_image *im)
{
	ELOG
	del_from_list(im);
	el_string_unref(im->pixels);
	mem_free(im);
}

#define EL_KITTY_CHUNK 4096

void
try_to_draw_k_images(struct terminal *term, struct string *text)
{
	ELOG

	add_to_string(text, "\033_Ga=d\033\\");

	if (!term->kitty) {
		return;
	}

	int i;

	for (i = 0; i < term->number_of_images; i++) {
		struct k_image *im = term->k_images[i];

		add_cursor_move_to_string(text, im->cy + 1, im->cx + 1);

		int id = get_id_from_ID(im->ID);

		if (id < 0) {
			int m;
			int left = im->pixels->length;
			int sent = 0;
			while (1) {
				m = left > EL_KITTY_CHUNK;
				if (!sent) {
					add_format_to_string(text, "\033_Gf=%d,I=%d,s=%d,v=%d,m=%d,t=d,a=T%s,x=%d,y=%d,w=%d,h=%d;", KITTY_BYTES_PER_PIXEL * 8, im->ID, im->width, im->height, m, (im->compressed ? ",o=z": ""), im->x, im->y, im->w, im->h);
				} else {
					add_format_to_string(text, "\033_Gm=%d;", m);
				}
				add_bytes_to_string(text, im->pixels->data + sent, m ? EL_KITTY_CHUNK : left);
				add_to_string(text, "\033\\");
				if (!m) {
					break;
				}
				sent += EL_KITTY_CHUNK;
				left -= EL_KITTY_CHUNK;
			};
		} else {
			add_format_to_string(text, "\033_Gi=%d,x=%d,y=%d,w=%d,h=%d,a=p,q=1\033\\", id, im->x, im->y, im->w, im->h);
		}
	}
}

#undef EL_KITTY_CHUNK

struct k_image *
copy_k_frame(struct k_image *src, struct el_box *box, int cell_width, int cell_height, int dx, int dy)
{
	ELOG
	struct k_image *dest = mem_calloc(1, sizeof(*dest));

	if (!dest) {
		return NULL;
	}
	dest->pixels = el_string_ref(src->pixels);

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
