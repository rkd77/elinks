#ifndef EL__TERMINAL_IMAGE_H
#define EL__TERMINAL_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

struct el_string;

#ifdef CONFIG_KITTY
struct el_string *el_kitty_get_image(char *data, int len, int *width, int *height, int *compressed);
#endif

#ifdef CONFIG_LIBSIXEL
char *el_sixel_get_image(char *data, int len, int *outlen);
#endif

#ifdef __cplusplus
}
#endif

#endif /* EL__TERMINAL_IMAGE_H */
