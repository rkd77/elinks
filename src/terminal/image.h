#ifndef EL__TERMINAL_IMAGE_H
#define EL__TERMINAL_IMAGE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_KITTY
unsigned char *el_kitty_get_image(char *data, int len, int *outlen, int *width, int *height, int *compressed);
#endif

#ifdef CONFIG_LIBSIXEL
unsigned char *el_sixel_get_image(char *data, int len, int *outlen);
#endif

#ifdef __cplusplus
}
#endif

#endif /* EL__TERMINAL_IMAGE_H */
