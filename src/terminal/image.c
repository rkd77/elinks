/* Terminal kitty and sixel routines. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>

#ifdef CONFIG_GZIP
#include <zlib.h>
#endif

#ifdef CONFIG_LIBSIXEL
#include <sixel.h>
#endif

#include "elinks.h"

#include "terminal/image.h"
#include "util/base64.h"
#include "util/memory.h"
#include "util/string.h"

#if defined(CONFIG_KITTY) || defined(CONFIG_LIBSIXEL)
#define STB_IMAGE_IMPLEMENTATION
#include "terminal/stb_image.h"
#endif

#ifdef CONFIG_KITTY
char *
el_kitty_get_image(char *data, int length, int *outlen, int *width, int *height, int *compressed)
{
	ELOG
	int comp;
	unsigned char *pixels = stbi_load_from_memory((unsigned char *)data, length, width, height, &comp, KITTY_BYTES_PER_PIXEL);
	unsigned char *b64;

	if (!pixels) {
		return NULL;
	}
	int size = *width * *height * KITTY_BYTES_PER_PIXEL;
	*compressed = 0;

#ifdef CONFIG_GZIP
	unsigned char *complace = (unsigned char *)mem_alloc(size);

	if (complace) {
		unsigned long compsize = size;
		int res = compress(complace, &compsize, pixels, size);

		if (res == Z_OK) {
			*compressed = 1;
			b64 = base64_encode_bin(complace, compsize, outlen);
			stbi_image_free(pixels);
			mem_free(complace);
			return (char *)b64;
		}
		mem_free(complace);
	}
#endif
	b64 = base64_encode_bin(pixels, size, outlen);
	stbi_image_free(pixels);

	return (char *)b64;
}
#endif

#ifdef CONFIG_LIBSIXEL
static int
sixel_write_callback(char *data, int size, void *priv)
{
	ELOG
	struct string *text = priv;

	add_bytes_to_string(text, data, size);
	return size;
}

char *
el_sixel_get_image(char *data, int length, int *outlen)
{
	ELOG
	int comp;
	int width;
	int height;
	unsigned char *pixels = stbi_load_from_memory((unsigned char *)data, length, &width, &height, &comp, 3);

	if (!pixels) {
		return NULL;
	}
	sixel_output_t *output = NULL;
	sixel_dither_t *dither = NULL;
	struct string ret;

	if (!init_string(&ret)) {
		goto end;
	}
	SIXELSTATUS status = sixel_output_new(&output, sixel_write_callback, &ret, NULL);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256);
	sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
	status = sixel_encode(pixels, width, height, 3, dither, output);
	char *outdata = memacpy(ret.source, ret.length);

	if (outdata) {
		*outlen = ret.length;
	}
	done_string(&ret);
end:
	stbi_image_free(pixels);

	if (output) {
		sixel_output_unref(output);
	}

	if (dither) {
		sixel_dither_unref(dither);
	}

	return outdata;
}
#endif
