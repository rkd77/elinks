/** Base64 encode/decode implementation.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "util/base64.h"
#include "util/error.h"
#include "util/memory.h"

static unsigned char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

unsigned char *
base64_encode(register unsigned char *in)
{
	assert(in && *in);
	if_assert_failed return NULL;

	return base64_encode_bin(in, strlen(in), NULL);
}

unsigned char *
base64_encode_bin(register unsigned char *in, int inlen, int *outlen)
{
	unsigned char *out;
	unsigned char *outstr;

	assert(in && *in);
	if_assert_failed return NULL;

	out = outstr = mem_alloc((inlen / 3) * 4 + 4 + 1);
	if (!out) return NULL;

	while (inlen >= 3) {
		*out++ = base64_chars[ *in >> 2 ];
		*out++ = base64_chars[ (*in << 4 | *(in + 1) >> 4) & 63 ];
		*out++ = base64_chars[ (*(in + 1) << 2 | *(in + 2) >> 6) & 63 ];
		*out++ = base64_chars[ *(in + 2) & 63 ];
		inlen -= 3; in += 3;
	}
	if (inlen == 1) {
		*out++ = base64_chars[ *in >> 2 ];
		*out++ = base64_chars[ *in << 4 & 63 ];
		*out++ = '=';
		*out++ = '=';
	}
	if (inlen == 2) {
		*out++ = base64_chars[ *in >> 2 ];
		*out++ = base64_chars[ (*in << 4 | *(in + 1) >> 4) & 63 ];
		*out++ = base64_chars[ (*(in + 1) << 2) & 63 ];
		*out++ = '=';
	}
	*out = 0;

	if (outlen)
		*outlen = out-outstr;

	return outstr;
}

/* Base64 decoding is used only with the CONFIG_FORMHIST or CONFIG_GSSAPI
   feature, so i'll #ifdef it */
#if  defined(CONFIG_FORMHIST) || defined(CONFIG_GSSAPI)

unsigned char *
base64_decode(register unsigned char *in)
{
	assert(in && *in);
	if_assert_failed return NULL;

	return base64_decode_bin(in, strlen(in), NULL);
}

/** Decode a Base64 string.
 * @param in		Input Base64 string
 * @param inlen		Length of @a in, in bytes
 * @param[out] outlen	Length of decoded string
 *
 * @returns the string decoded (must be freed by the caller)
 * or NULL if an error occurred (syntax error or out of memory) */
unsigned char *
base64_decode_bin(register unsigned char *in, int inlen, int *outlen)
{
	static unsigned char is_base64_char[256]; /* static to force initialization at zero */
	static unsigned char decode[256];
	unsigned char *out;
	unsigned char *outstr;
	int count = 0;
	unsigned int bits = 0;
	static int once = 0;

	assert(in && *in);
	if_assert_failed return NULL;

	outstr = out = mem_alloc(inlen / 4 * 3 + 1);
	if (!outstr) return NULL;

	if (!once) {
		int i = sizeof(base64_chars) - 1;

		while (i >= 0) {
			is_base64_char[base64_chars[i]] = 1;
			decode[base64_chars[i]] = i;
			i--;
		}
		once = 1;
	}

	while (*in) {
		if (*in == '=') break;
		if (!is_base64_char[*in])
			goto decode_error;

		bits += decode[*in];
		count++;
		if (count == 4) {
			*out++ = bits >> 16;
			*out++ = (bits >> 8) & 0xff;
			*out++ = bits & 0xff;
			bits = 0;
			count = 0;
		} else {
			bits <<= 6;
		}

		++in;
	}

	if (!*in) {
		if (count) goto decode_error;
	} else { /* '=' */
		switch (count) {
			case 1:
				goto decode_error;
				break;
			case 2:
				*out++ = bits >> 10;
				break;
			case 3:
				*out++ = bits >> 16;
				*out++ = (bits >> 8) & 0xff;
				break;
		}
	}

	*out = 0;

	if (outlen)
		*outlen = out-outstr;

	return outstr;

decode_error:
	mem_free(outstr);
	return NULL;
}

#endif /* CONFIG_FORMHIST */
