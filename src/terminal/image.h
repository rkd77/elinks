#ifndef EL__TERMINAL_IMAGE_H
#define EL__TERMINAL_IMAGE_H

#ifdef CONFIG_LIBSIXEL
#include <sixel.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct el_string;

#ifdef CONFIG_KITTY
struct el_string *el_kitty_get_image(char *data, int len, int *width, int *height, int *compressed);
#endif

#ifdef CONFIG_LIBSIXEL
extern sixel_allocator_t *el_sixel_allocator;
char *el_sixel_get_image(char *data, int len, int *outlen);

#ifdef CONFIG_MEMCOUNT
void init_sixel_allocator(void);
#endif

#endif

#ifdef __cplusplus
}
#endif

#endif /* EL__TERMINAL_IMAGE_H */
