#ifndef EL__UTIL_BASE64_H
#define EL__UTIL_BASE64_H

#ifdef __cplusplus
extern "C" {
#endif

unsigned char *base64_encode(unsigned char *);
unsigned char *base64_decode(unsigned char *);

unsigned char *base64_encode_bin(unsigned char *, int, int *);
unsigned char *base64_decode_bin(unsigned char *, int, int *);

#ifdef __cplusplus
}
#endif

#endif
