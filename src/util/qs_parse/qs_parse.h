#include <string.h>

#ifndef BG_QSPARSE_H_
#define BG_QSPARSE_H_

#ifdef __cplusplus
extern "C" {
#endif

/*  string.h needed for strcspn() and strlen()  */


/*  Similar to strncmp, but handles URL-encoding for either string  */
int qs_strncmp(const char * s, const char * qs, register size_t n);


/*  Finds the beginning of each key/value pair and stores a pointer in qs_kv.
 *  Also decodes the value portion of the k/v pair *in-place*.  In a future
 *  enhancement it will also have a compile-time option of sorting qs_kv
 *  alphabetically by key.  */
int qs_parse(char * qs, char * qs_kv[], int qs_kv_size);


/*  Used by qs_parse to decode the value portion of a k/v pair  */
int qs_decode(char * qs);


/*  Looks up the value according to the key on a pre-processed query string
 *  A future enhancement will be a compile-time option to look up the key
 *  in a pre-sorted qs_kv array via a binary search.  */
char * qs_k2v(const char * key, char * qs_kv[], int qs_kv_size);


/*  Non-destructive lookup of value, based on key.  User provides the
 *  destinaton string and length.  */
char * qs_scanvalue(const char * key, const char * qs, char * val, size_t val_len);


/*  Converts the 3 or 6 (RGB), or 4 or 8 (RGBA) hex chars in the color string
 *  to double values in the range 0.0-1.0.  Returns the number of converted
 *  chars.  */
int hex2dcolor(char * color, double * r, double * g, double * b, double * a);


/*  Converts the 3/6 (RGB) or 4/8 (RGBA) hex chars in the color string to
 *  values spanning the full range of unsigned char, 0-255.  Returns the
 *  number of converted chars.  */
int hex2ccolor(char * color, unsigned char * r, unsigned char * g, unsigned char * b, unsigned char * a);

#ifdef __cplusplus
}
#endif 

#endif  // BG_QSPARSE_H_
