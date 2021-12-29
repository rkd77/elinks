#include "qs_parse.h"
#include <stdio.h>

// TODO: implement sorting of the qs_kv array; for now ensure it's not compiled
#undef _qsSORTING

// isxdigit _is_ available in <ctype.h>, but let's avoid another header instead
#define ISHEX(x)    ((((x)>='0'&&(x)<='9') || ((x)>='A'&&(x)<='F') || ((x)>='a'&&(x)<='f')) ? 1 : 0)
#define HEX2DEC(x)  (((x)>='0'&&(x)<='9') ? (x)-48 : ((x)>='A'&&(x)<='F') ? (x)-55 : ((x)>='a'&&(x)<='f') ? (x)-87 : 0)
#define ISQSCHR(x) ((((x)=='=')||((x)=='#')||((x)=='&')||((x)=='\0')) ? 0 : 1)

int qs_strncmp(const char * s, const char * qs, register size_t n)
{
    int i=0;
    register unsigned char u1, u2, unyb, lnyb;

    while(n-- > 0)
    {
        u1 = (unsigned char) *s++;
        u2 = (unsigned char) *qs++;

        if ( ! ISQSCHR(u1) ) {  u1 = '\0';  }
        if ( ! ISQSCHR(u2) ) {  u2 = '\0';  }

        if ( u1 == '+' ) {  u1 = ' ';  }
        if ( u1 == '%' ) // easier/safer than scanf
        {
            unyb = (unsigned char) *s++;
            lnyb = (unsigned char) *s++;
            if ( ISHEX(unyb) && ISHEX(lnyb) )
                u1 = (HEX2DEC(unyb) * 16) + HEX2DEC(lnyb);
            else
                u1 = '\0';
        }

        if ( u2 == '+' ) {  u2 = ' ';  }
        if ( u2 == '%' ) // easier/safer than scanf
        {
            unyb = (unsigned char) *qs++;
            lnyb = (unsigned char) *qs++;
            if ( ISHEX(unyb) && ISHEX(lnyb) )
                u2 = (HEX2DEC(unyb) * 16) + HEX2DEC(lnyb);
            else
                u2 = '\0';
        }

        if ( u1 != u2 )
            return u1 - u2;
        if ( u1 == '\0' )
            return 0;
        i++;
    }
    if ( ISQSCHR(*qs) )
        return -1;
    else
        return 0;
}


int qs_parse(char * qs, char * qs_kv[], int qs_kv_size)
{
    int i, j;
    char * substr_ptr;

    for(i=0; i<qs_kv_size; i++)  qs_kv[i] = NULL;

    // find the beginning of the k/v substrings
    if ( (substr_ptr = strchr(qs, '?')) != NULL )
        substr_ptr++;
    else
        substr_ptr = qs;

    i=0;
    while(i<qs_kv_size)
    {
        qs_kv[i] = substr_ptr;
        j = strcspn(substr_ptr, "&");
        if ( substr_ptr[j] == '\0' ) {  break;  }
        substr_ptr += j + 1;
        i++;
    }
    i++;  // x &'s -> means x iterations of this loop -> means *x+1* k/v pairs

    // we only decode the values in place, the keys could have '='s in them
    // which will hose our ability to distinguish keys from values later
    for(j=0; j<i; j++)
    {
        substr_ptr = qs_kv[j] + strcspn(qs_kv[j], "=&#");
        if ( substr_ptr[0] == '&' )  // blank value: skip decoding
            substr_ptr[0] = '\0';
        else
            qs_decode(++substr_ptr);
    }

#ifdef _qsSORTING
// TODO: qsort qs_kv, using qs_strncmp() for the comparison
#endif

    return i;
}


int qs_decode(char * qs)
{
    int i=0, j=0;

    while( ISQSCHR(qs[j]) )
    {
        if ( qs[j] == '+' ) {  qs[i] = ' ';  }
        else if ( qs[j] == '%' ) // easier/safer than scanf
        {
            if ( ! ISHEX(qs[j+1]) || ! ISHEX(qs[j+2]) )
            {
                qs[i] = '\0';
                return i;
            }
            qs[i] = (HEX2DEC(qs[j+1]) * 16) + HEX2DEC(qs[j+2]);
            j+=2;
        }
        else
        {
            qs[i] = qs[j];
        }
        i++;  j++;
    }
    qs[i] = '\0';

    return i;
}


char * qs_k2v(const char * key, char * qs_kv[], int qs_kv_size)
{
    int i;
    size_t key_len, skip;

    key_len = strlen(key);

#ifdef _qsSORTING
// TODO: binary search for key in the sorted qs_kv
#else  // _qsSORTING
    for(i=0; i<qs_kv_size; i++)
    {
        // we rely on the unambiguous '=' to find the value in our k/v pair
        if ( qs_strncmp(key, qs_kv[i], key_len) == 0 )
        {
            skip = strcspn(qs_kv[i], "=");
            if ( qs_kv[i][skip] == '=' )
                skip++;
            // return (zero-char value) ? ptr to trailing '\0' : ptr to value
            return qs_kv[i] + skip;
        }
    }
#endif  // _qsSORTING

    return NULL;
}


char * qs_scanvalue(const char * key, const char * qs, char * val, size_t val_len)
{
    int i, key_len;
    char * tmp;

    // find the beginning of the k/v substrings
    if ( (tmp = strchr(qs, '?')) != NULL )
        qs = tmp + 1;

    key_len = strlen(key);
    while(qs[0] != '#' && qs[0] != '\0')
    {
        if ( qs_strncmp(key, qs, key_len) == 0 )
            break;
        qs += strcspn(qs, "&") + 1;
    }

    if ( qs[0] == '\0' ) return NULL;

    qs += strcspn(qs, "=&#");
    if ( qs[0] == '=' )
    {
        qs++;
        i = strcspn(qs, "&=#");
        strncpy(val, qs, (val_len-1)<(i+1) ? (val_len-1) : (i+1));
        qs_decode(val);
    }
    else
    {
        if ( val_len > 0 )
            val[0] = '\0';
    }

    return val;
}


int hex2dcolor(char * color, double * r, double * g, double * b, double * a)
{
    int i, j;

    if ( color == NULL ) return 0;

    i = strlen(color);
    if ( i != 8 && i != 6 && i != 4 && i != 3 )  return 0;
    for(j=0; j<i; j++)  if ( ! ISHEX(color[j]) )  return 0;
    switch(i)
    {
        // (H*16+H)/255 ==  H*17/255 == H/15
        case 3:
            *r = HEX2DEC(color[0]) / 15.0;
            *g = HEX2DEC(color[1]) / 15.0;
            *b = HEX2DEC(color[2]) / 15.0;
            break;
        case 4:
            *r = HEX2DEC(color[0]) / 15.0;
            *g = HEX2DEC(color[1]) / 15.0;
            *b = HEX2DEC(color[2]) / 15.0;
            *a = HEX2DEC(color[3]) / 15.0;
            break;
        case 6:
            *r = ((HEX2DEC(color[0]) * 16) + HEX2DEC(color[1])) / 255.0;
            *g = ((HEX2DEC(color[2]) * 16) + HEX2DEC(color[3])) / 255.0;
            *b = ((HEX2DEC(color[4]) * 16) + HEX2DEC(color[5])) / 255.0;
            break;
        case 8:
            *r = ((HEX2DEC(color[0]) * 16) + HEX2DEC(color[1])) / 255.0;
            *g = ((HEX2DEC(color[2]) * 16) + HEX2DEC(color[3])) / 255.0;
            *b = ((HEX2DEC(color[4]) * 16) + HEX2DEC(color[5])) / 255.0;
            *a = ((HEX2DEC(color[6]) * 16) + HEX2DEC(color[7])) / 255.0;
            break;
    }
    return i;
}


int hex2ccolor(char * color, unsigned char * r, unsigned char * g, unsigned char * b, unsigned char * a)
{
    int i, j;

    if ( color == NULL ) return 0;

    i = strlen(color);
    if ( i != 8 && i != 6 && i != 4 && i != 3 )  return 0;
    for(j=0; j<i; j++)  if ( ! ISHEX(color[j]) )  return 0;
    switch(i)
    {
        // (H*16+H) == H*17
        case 3:
            *r = HEX2DEC(color[0]) * 17;
            *g = HEX2DEC(color[1]) * 17;
            *b = HEX2DEC(color[2]) * 17;
            break;
        case 4:
            *r = HEX2DEC(color[0]) * 17;
            *g = HEX2DEC(color[1]) * 17;
            *b = HEX2DEC(color[2]) * 17;
            *a = HEX2DEC(color[3]) * 17;
            break;
        case 6:
            *r = (HEX2DEC(color[0]) * 16) + HEX2DEC(color[1]);
            *g = (HEX2DEC(color[2]) * 16) + HEX2DEC(color[3]);
            *b = (HEX2DEC(color[4]) * 16) + HEX2DEC(color[5]);
            break;
        case 8:
            *r = (HEX2DEC(color[0]) * 16) + HEX2DEC(color[1]);
            *g = (HEX2DEC(color[2]) * 16) + HEX2DEC(color[3]);
            *b = (HEX2DEC(color[4]) * 16) + HEX2DEC(color[5]);
            *a = (HEX2DEC(color[6]) * 16) + HEX2DEC(color[7]);
            break;
    }
    return i;
}
