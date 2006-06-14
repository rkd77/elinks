#ifndef EL__UTIL_BASE64_H
#define EL__UTIL_BASE64_H

unsigned char *base64_encode(unsigned char *);
unsigned char *base64_decode(unsigned char *);

unsigned char *base64_encode_bin(unsigned char *, int, int *);
unsigned char *base64_decode_bin(unsigned char *, int, int *);

#endif
