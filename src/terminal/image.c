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

#ifdef CONFIG_LIBWEBP
#include <webp/decode.h>
#endif

#ifdef CONFIG_LIBAVIF
#include <avif/avif.h>
#endif

#include "elinks.h"

#include "terminal/image.h"
#ifdef CONFIG_KITTY
#include "terminal/kitty.h"
#endif
#include "util/base64.h"
#include "util/memcount.h"
#include "util/memory.h"
#include "util/string.h"

#if defined(CONFIG_KITTY) || defined(CONFIG_LIBSIXEL)
#ifdef CONFIG_MEMCOUNT

#define STBI_MALLOC(sz)           el_stbi_malloc(sz)
#define STBI_REALLOC(p,newsz)     el_stbi_realloc(p,newsz)
#define STBI_FREE(p)              el_stbi_free(p)

#endif
#define STB_IMAGE_IMPLEMENTATION
#include "terminal/stb_image.h"
#endif

#ifdef CONFIG_LIBAVIF
static avifResult
el_avifRGBImageAllocatePixels(avifRGBImage *rgb)
{
	const uint32_t pixelSize = avifRGBImagePixelSize(rgb);

	if (rgb->width > UINT32_MAX / pixelSize) {
		return AVIF_RESULT_INVALID_ARGUMENT;
	}
	const uint32_t rowBytes = rgb->width * pixelSize;

	if (rgb->height > PTRDIFF_MAX / rowBytes) {
		return AVIF_RESULT_INVALID_ARGUMENT;
	}
	rgb->pixels = (uint8_t *)mem_alloc((size_t)rowBytes * rgb->height);

	if (!rgb->pixels) {
		return AVIF_RESULT_OUT_OF_MEMORY;
	}
	rgb->rowBytes = rowBytes;

	return AVIF_RESULT_OK;
}

static unsigned char *
eldecode_avif(const unsigned char *data, int length, int *width, int *height, int typ)
{
	avifRGBImage rgb;
	memset(&rgb, 0, sizeof(rgb));
	avifDecoder *decoder = avifDecoderCreate();

	if (!decoder) {
		return NULL;
	}
	avifResult result = avifDecoderSetIOMemory(decoder, data, (long)length);

	if (result != AVIF_RESULT_OK) {
		goto cleanup;
	}
	result = avifDecoderParse(decoder);

	if (result != AVIF_RESULT_OK) {
		goto cleanup;
	}
	result = avifDecoderNextImage(decoder);

	if (result != AVIF_RESULT_OK) {
		goto cleanup;
	}

	avifRGBImageSetDefaults(&rgb, decoder->image);
	rgb.depth = 8;
	rgb.format = (typ == 4 ? AVIF_RGB_FORMAT_RGBA : AVIF_RGB_FORMAT_RGB);
	*width = decoder->image->width;
	*height = decoder->image->height;
	result = el_avifRGBImageAllocatePixels(&rgb);

	if (result != AVIF_RESULT_OK) {
		goto cleanup;
	}
	result = avifImageYUVToRGB(decoder->image, &rgb);

	if (result != AVIF_RESULT_OK) {
		goto cleanup;
	}
cleanup:
	avifDecoderDestroy(decoder);

	if (result != AVIF_RESULT_OK) {
		mem_free_if(rgb.pixels);
		return NULL;
	}

	return rgb.pixels;
}
#endif

#ifdef CONFIG_KITTY
struct el_string *
el_kitty_get_image(char *data, int length, int *width, int *height, int *compressed)
{
	ELOG
	int comp;
	int outlen = 0;
	int webp = 0;
	int avif = 0;
	unsigned char *pixels = stbi_load_from_memory((unsigned char *)data, length, width, height, &comp, KITTY_BYTES_PER_PIXEL);
	unsigned char *b64;

	if (!pixels) {
#ifdef CONFIG_LIBWEBP
		pixels = WebPDecodeRGBA((const uint8_t*)data, length, width, height);
		webp = 1;
#endif

#ifdef CONFIG_LIBAVIF
		if (!pixels) {
			pixels = eldecode_avif((const uint8_t*)data, length, width, height, 4);
			avif = 1;
		}
#endif
		if (!pixels) {
			return NULL;
		}
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
			b64 = base64_encode_bin(complace, compsize, &outlen);

			if (webp) {
#ifdef CONFIG_LIBWEBP
				WebPFree(pixels);
#endif
			} else if (avif) {
				mem_free(pixels);
			} else {
				stbi_image_free(pixels);
			}
			mem_free(complace);

			return (b64 ? el_string_init((char *)b64, (unsigned int)outlen) : NULL);
		}
		mem_free(complace);
	}
#endif
	b64 = base64_encode_bin(pixels, size, &outlen);

	if (webp) {
#ifdef CONFIG_LIBWEBP
		WebPFree(pixels);
#endif
	} else if (avif) {
		mem_free(pixels);
	} else {
		stbi_image_free(pixels);
	}
	return (b64 ? el_string_init((char *)b64, (unsigned int)outlen) : NULL);
}
#endif

#ifdef CONFIG_LIBSIXEL

#ifdef CONFIG_MEMCOUNT
sixel_allocator_t *
init_sixel_allocator(void)
{
	ELOG

	sixel_allocator_t *el_sixel_allocator = NULL;
	sixel_allocator_new(&el_sixel_allocator, el_sixel_malloc, el_sixel_calloc, el_sixel_realloc, el_sixel_free);

	return el_sixel_allocator;
}
#endif

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
	char *outdata = NULL;
	int webp = 0;
	int avif = 0;

	if (!pixels) {
#ifdef CONFIG_LIBWEBP
		pixels = WebPDecodeRGB((const uint8_t*)data, length, &width, &height);
		webp = 1;
#endif
#ifdef CONFIG_LIBAVIF
		if (!pixels) {
			pixels = eldecode_avif((const uint8_t*)data, length, &width, &height, 3);
			avif = 1;
		}
#endif
		if (!pixels) {
			return NULL;
		}
	}
	sixel_output_t *output = NULL;
	sixel_dither_t *dither = NULL;
	sixel_allocator_t *el_sixel_allocator = NULL;
	struct string ret;

	if (!init_string(&ret)) {
		goto end;
	}
#ifdef CONFIG_MEMCOUNT
	el_sixel_allocator = init_sixel_allocator();
#endif
	SIXELSTATUS status = sixel_output_new(&output, sixel_write_callback, &ret, el_sixel_allocator);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	dither = sixel_dither_get(SIXEL_BUILTIN_XTERM256);
	sixel_dither_set_pixelformat(dither, SIXEL_PIXELFORMAT_RGB888);
	status = sixel_encode(pixels, width, height, 3, dither, output);
	outdata = memacpy(ret.source, ret.length);

	if (outdata) {
		*outlen = ret.length;
	}
	done_string(&ret);
end:
	if (webp) {
#ifdef CONFIG_LIBWEBP
		WebPFree(pixels);
#endif
	} else if (avif) {
		mem_free(pixels);
	} else {
		stbi_image_free(pixels);
	}
	sixel_output_unref(output);
	sixel_dither_unref(dither);
	sixel_allocator_unref(el_sixel_allocator);

	return outdata;
}
#endif
