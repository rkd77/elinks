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

#ifdef CONFIG_LIBRSVG
#include <librsvg/rsvg.h>
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

#ifdef CONFIG_LIBRSVG
static inline void
el_cairo_argb32_pixel_to_rgba(
	const uint8_t *src,  // BGRA
	uint8_t *dst         // RGBA
)
{
	uint8_t b = src[0];
	uint8_t g = src[1];
	uint8_t r = src[2];
	uint8_t a = src[3];

	// un-premultiply
	if (a != 0) {
		r = (uint8_t)((r * 255 + a / 2) / a);
		g = (uint8_t)((g * 255 + a / 2) / a);
		b = (uint8_t)((b * 255 + a / 2) / a);
	}

	dst[0] = r;
	dst[1] = g;
	dst[2] = b;
	dst[3] = a;
}

static inline void
el_cairo_rgb24_pixel_to_rgb(
	const uint8_t *src,  // B G R X
	uint8_t *dst         // R G B
)
{
	dst[0] = src[2]; // R
	dst[1] = src[1]; // G
	dst[2] = src[0]; // B
}

static unsigned char *
eldecode_rsvg(const unsigned char *data, int length, int *width, int *height, int typ)
{
	GError *error = NULL;
	RsvgHandle *handle = rsvg_handle_new_from_data(data, length, &error);

	if (!handle) {
		return NULL;
	}
	uint8_t *dst = NULL;
	int ret = 0;
	cairo_surface_t *surface = NULL;
	cairo_t *cr = NULL;
	RsvgLength out_width, out_height;

	gboolean out_has_width = 0;
	gboolean out_has_height = 0;
	gboolean out_has_viewbox = 0;
	RsvgRectangle out_viewbox;

	rsvg_handle_get_intrinsic_dimensions(handle, &out_has_width, &out_width, &out_has_height, &out_height, &out_has_viewbox, &out_viewbox);

	if (!out_has_width || !out_has_height || out_has_viewbox) {
		if (!out_has_viewbox) {
			return NULL;
		}
		out_width.length  = out_viewbox.width;
		out_height.length = out_viewbox.height;
	}
	*width = (int)out_width.length;
	*height = (int)out_height.length;
	surface = cairo_image_surface_create((typ == 4 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24), *width, *height);

	if (!surface) {
		goto cleanup;
	}
	cr = cairo_create(surface);

	if (!cr) {
		goto cleanup;
	}
	/* Set the dots-per-inch */
	rsvg_handle_set_dpi(handle, 96.0);

	/* Render the handle scaled proportionally into that whole surface */
	RsvgRectangle viewport = {
		.x = 0.0,
		.y = 0.0,
		.width = out_width.length,
		.height = out_height.length,
	};

	if (!rsvg_handle_render_document(handle, cr, &viewport, &error)) {
		goto cleanup;
	}
	ret = 1;
	cairo_surface_flush(surface);
cleanup:
	cairo_destroy(cr);
	g_object_unref(handle);

	if (ret) {
		const uint8_t *src = cairo_image_surface_get_data(surface);
		int size = *width * *height;
		dst = mem_alloc(size * typ);
		int i;

		if (dst) {
			if (typ == 4) {
				for (i = 0; i < size; i++) {
					el_cairo_argb32_pixel_to_rgba(src + i * 4, dst + i * 4);
				}
			} else {
				for (i = 0; i < size; i++) {
					el_cairo_rgb24_pixel_to_rgb(src + i * 4, dst + i * 3);
				}
			}
		}
	}
	cairo_surface_destroy(surface);
	return dst;
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
	int rsvg = 0;
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

#ifdef CONFIG_LIBRSVG
		if (!pixels) {
			pixels = eldecode_rsvg((const uint8_t*)data, length, width, height, 4);
			rsvg = 1;
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
			} else if (avif || rsvg) {
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
	} else if (avif || rsvg) {
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
	int rsvg = 0;

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
#ifdef CONFIG_LIBRSVG
		if (!pixels) {
			pixels = eldecode_rsvg((const uint8_t*)data, length, &width, &height, 3);
			rsvg = 1;
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
	} else if (avif || rsvg) {
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
