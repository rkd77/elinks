#ifndef EL__UTIL_BASE64_H
#define EL__UTIL_BASE64_H

#ifdef __cplusplus
extern "C" {
#endif

char *base64_encode(char *);
char *base64_decode(char *);

char *base64_encode_bin(char *, int, int *);
char *base64_decode_bin(char *, int, int *);

#ifdef __cplusplus
}
#endif

#endif
