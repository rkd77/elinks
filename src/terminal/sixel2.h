#ifndef EL__TERMINAL_SIXEL2_H
#define EL__TERMINAL_SIXEL2_H

#ifdef __cplusplus
extern "C" {
#endif

struct string;

void encode(struct string *outs, unsigned char *img, int width, int height, int offx, int offy, int cropw, unsigned int palette);

#ifdef __cplusplus
}
#endif

#endif /* EL__TERMINAL_SIXEL2_H */
