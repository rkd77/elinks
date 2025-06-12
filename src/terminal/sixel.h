#ifndef EL__TERMINAL_SIXEL_H
#define EL__TERMINAL_SIXEL_H

#include <sixel.h>
#include "util/lists.h"
#include "util/string.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_LIBSIXEL
struct document;
struct el_box;
struct terminal;


struct image {
	LIST_HEAD_EL(struct image);
	struct string pixels;
	int x;
	int y;
	int width;
	int height;
	int image_number;
};

void delete_image(struct image *im);

void try_to_draw_images(struct terminal *term, struct string *text);

/* return height of image in terminal rows */
int add_image_to_document(struct document *doc, struct string *pixels, int lineno, struct image **imagine);

struct image *copy_frame(struct image *src, struct el_box *box, int cell_width, int cell_height, int dx, int dy);

#endif

#ifdef __cplusplus
}
#endif

#endif /* EL__TERMINAL_SIXEL_H */
