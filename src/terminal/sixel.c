/** Terminal sixel routines.
 * @file */

/*
 * Copyright (c) 2021 libsixel developers. See `AUTHORS`.
 * Copyright (c) 2014-2019 Hayaki Saito
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_MEMFD_CREATE
#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <sys/mman.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "elinks.h"

#include "document/document.h"
#include "osdep/osdep.h"
#include "terminal/hardio.h"
#include "terminal/screen.h"
#include "terminal/sixel.h"
#include "terminal/terminal.h"
#include "util/memcount.h"

/* encode settings object */
struct sixel_decoder {
	unsigned int ref;
	char *input;
	char *output;
	sixel_allocator_t *allocator;
};

/* encoder object */
struct sixel_encoder {
    unsigned int ref;               /* reference counter */
    sixel_allocator_t *allocator;   /* allocator object */
    int reqcolors;
    int color_option;
    char *mapfile;
    int builtin_palette;
    int method_for_diffuse;
    int method_for_largest;
    int method_for_rep;
    int quality_mode;
    int method_for_resampling;
    int loop_mode;
    int palette_type;
    int f8bit;
    int finvert;
    int fuse_macro;
    int fignore_delay;
    int complexion;
    int fstatic;
    int pixelwidth;
    int pixelheight;
    int percentwidth;
    int percentheight;
    int clipx;
    int clipy;
    int clipwidth;
    int clipheight;
    int clipfirst;
    int macro_number;
    int penetrate_multiplexer;
    int encode_policy;
    int ormode;
    int pipe_mode;
    int verbose;
    int has_gri_arg_limit;
    unsigned char *bgcolor;
    int outfd;
    int finsecure;
    int *cancel_flag;
    void *dither_cache;
};

#ifdef CONFIG_MEMCOUNT
static sixel_allocator_t *el_sixel_allocator;

static void
init_allocator(void)
{
	static int initialized = 0;
	if (!initialized) {
		sixel_allocator_new(&el_sixel_allocator, el_sixel_malloc, el_sixel_calloc, el_sixel_realloc, el_sixel_free);
		initialized = 1;
	}
}
#endif

/* palette type */
#define SIXEL_COLOR_OPTION_DEFAULT          0   /* use default settings */
#define SIXEL_COLOR_OPTION_MONOCHROME       1   /* use monochrome palette */
#define SIXEL_COLOR_OPTION_BUILTIN          2   /* use builtin palette */
#define SIXEL_COLOR_OPTION_MAPFILE          3   /* use mapfile option */
#define SIXEL_COLOR_OPTION_HIGHCOLOR        4   /* use highcolor option */


static int
sixel_write_callback(char *data, int size, void *priv)
{
	struct string *text = priv;

	add_bytes_to_string(text, data, size);
	return size;
}

static SIXELSTATUS
sixel_encoder_output_without_macro(
    sixel_frame_t       /* in */ *frame,
    sixel_dither_t      /* in */ *dither,
    sixel_output_t      /* in */ *output,
    sixel_encoder_t     /* in */ *encoder)
{
    SIXELSTATUS status = SIXEL_OK;
    static unsigned char *p;
    int depth;
    enum { message_buffer_size = 256 };
    char message[message_buffer_size];
    int nwrite;
    int dulation;
    int delay;
    int lag = 0;
    struct timespec tv;
    clock_t start;
    unsigned char *pixbuf;
    int width;
    int height;
    int pixelformat;
    size_t size;

    if (encoder == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_without_macro: encoder object is null.");
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (encoder->color_option == SIXEL_COLOR_OPTION_DEFAULT) {
        sixel_dither_set_optimize_palette(dither, 1);
    }

    pixelformat = sixel_frame_get_pixelformat(frame);
    depth = sixel_helper_compute_depth(pixelformat);
    if (depth < 0) {
        status = SIXEL_LOGIC_ERROR;
        nwrite = sprintf(message,
                         "sixel_encoder_output_without_macro: "
                         "sixel_helper_compute_depth(%08x) failed.",
                         pixelformat);
        if (nwrite > 0) {
            sixel_helper_set_additional_message(message);
        }
        goto end;
    }

    width = sixel_frame_get_width(frame);
    height = sixel_frame_get_height(frame);
    size = (size_t)(width * height * depth);
    p = (unsigned char *)sixel_allocator_malloc(encoder->allocator, size);
    if (p == NULL) {
        sixel_helper_set_additional_message(
            "sixel_encoder_output_without_macro: sixel_allocator_malloc() failed.");
        status = SIXEL_BAD_ALLOCATION;
        goto end;
    }
    start = clock();
    delay = sixel_frame_get_delay(frame);
    if (delay > 0 && !encoder->fignore_delay) {
        dulation = (int)((clock() - start) * 1000 * 1000 / CLOCKS_PER_SEC) - (int)lag;
        lag = 0;
        if (dulation < 10000 * delay) {
            tv.tv_sec = 0;
            tv.tv_nsec = (long)((10000 * delay - dulation) * 1000);
            nanosleep(&tv, NULL);
        } else {
            lag = (int)(10000 * delay - dulation);
        }
    }

    pixbuf = sixel_frame_get_pixels(frame);
    memcpy(p, pixbuf, (size_t)(width * height * depth));

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        goto end;
    }

    status = sixel_encode(p, width, height, depth, dither, output);
    if (status != SIXEL_OK) {
        goto end;
    }

end:
    sixel_allocator_free(encoder->allocator, p);

    return status;
}


static SIXELSTATUS
sixel_encoder_output_with_macro(
    sixel_frame_t   /* in */ *frame,
    sixel_dither_t  /* in */ *dither,
    sixel_output_t  /* in */ *output,
    sixel_encoder_t /* in */ *encoder)
{
    SIXELSTATUS status = SIXEL_OK;
    enum { message_buffer_size = 256 };
    char buffer[message_buffer_size];
    int nwrite;
    int dulation;
    int lag = 0;
    struct timespec tv;
    clock_t start;
    unsigned char *pixbuf;
    int width;
    int height;
    int delay;

    start = clock();
    if (sixel_frame_get_loop_no(frame) == 0) {
        if (encoder->macro_number >= 0) {
            nwrite = sprintf(buffer, "\033P%d;0;1!z", encoder->macro_number);
        } else {
            nwrite = sprintf(buffer, "\033P%d;0;1!z", sixel_frame_get_frame_no(frame));
        }
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: sprintf() failed.");
            goto end;
        }
        nwrite = sixel_write_callback(buffer, (int)strlen(buffer), &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: sixel_write_callback() failed.");
            goto end;
        }

        pixbuf = sixel_frame_get_pixels(frame),
        width = sixel_frame_get_width(frame),
        height = sixel_frame_get_height(frame),
        status = sixel_encode(pixbuf, width, height, /* unused */ 3, dither, output);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        nwrite = sixel_write_callback("\033\\", 2, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: sixel_write_callback() failed.");
            goto end;
        }
    }
    if (encoder->macro_number < 0) {
        nwrite = sprintf(buffer, "\033[%d*z", sixel_frame_get_frame_no(frame));
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: sprintf() failed.");
        }
        nwrite = sixel_write_callback(buffer, (int)strlen(buffer), &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "sixel_encoder_output_with_macro: sixel_write_callback() failed.");
            goto end;
        }
        delay = sixel_frame_get_delay(frame);
        if (delay > 0 && !encoder->fignore_delay) {
            dulation = (int)((clock() - start) * 1000 * 1000 / CLOCKS_PER_SEC) - (int)lag;
            lag = 0;
            if (dulation < 10000 * delay) {
                tv.tv_sec = 0;
                tv.tv_nsec = (long)((10000 * delay - dulation) * 1000);
                nanosleep(&tv, NULL);
            } else {
                lag = (int)(10000 * delay - dulation);
            }
        }
    }

end:
    return status;
}


/* returns monochrome dithering context object */
static SIXELSTATUS
sixel_prepare_monochrome_palette(
    sixel_dither_t  /* out */ **dither,
     int            /* in */  finvert)
{
    SIXELSTATUS status = SIXEL_FALSE;

    if (finvert) {
        *dither = sixel_dither_get(SIXEL_BUILTIN_MONO_LIGHT);
    } else {
        *dither = sixel_dither_get(SIXEL_BUILTIN_MONO_DARK);
    }
    if (*dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prepare_monochrome_palette: sixel_dither_get() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}


/* returns dithering context object with specified builtin palette */
static SIXELSTATUS
sixel_prepare_builtin_palette(
    sixel_dither_t /* out */ **dither,
    int            /* in */  builtin_palette)
{
    SIXELSTATUS status = SIXEL_FALSE;

    *dither = sixel_dither_get(builtin_palette);
    if (*dither == NULL) {
        sixel_helper_set_additional_message(
            "sixel_prepare_builtin_palette: sixel_dither_get() failed.");
        status = SIXEL_RUNTIME_ERROR;
        goto end;
    }

    status = SIXEL_OK;

end:
    return status;
}

#if 0
/* create palette from specified map file */
static SIXELSTATUS
sixel_prepare_specified_palette(
    sixel_dither_t  /* out */   **dither,
    sixel_encoder_t /* in */    *encoder)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_callback_context_for_mapfile_t callback_context;

    callback_context.reqcolors = encoder->reqcolors;
    callback_context.dither = NULL;
    callback_context.allocator = encoder->allocator;

    status = sixel_helper_load_image_file(encoder->mapfile,
                                          1,   /* fstatic */
                                          1,   /* fuse_palette */
                                          SIXEL_PALETTE_MAX, /* reqcolors */
                                          encoder->bgcolor,
                                          SIXEL_LOOP_DISABLE,
                                          load_image_callback_for_palette,
                                          encoder->finsecure,
                                          encoder->cancel_flag,
                                          &callback_context,
                                          encoder->allocator);
    if (status != SIXEL_OK) {
        return status;
    }

    *dither = callback_context.dither;

    return status;
}
#endif

/* create dither object from a frame */
static SIXELSTATUS
sixel_encoder_prepare_palette(
    sixel_encoder_t *encoder,  /* encoder object */
    sixel_frame_t   *frame,    /* input frame object */
    sixel_dither_t  **dither)  /* dither object to be created from the frame */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int histogram_colors;

    switch (encoder->color_option) {
    case SIXEL_COLOR_OPTION_HIGHCOLOR:
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_dither_new(dither, (-1), encoder->allocator);
        }
        goto end;
    case SIXEL_COLOR_OPTION_MONOCHROME:
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_monochrome_palette(dither, encoder->finvert);
        }
        goto end;
    case SIXEL_COLOR_OPTION_MAPFILE:
#if 0
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_specified_palette(dither, encoder);
        }
#endif
        goto end;
    case SIXEL_COLOR_OPTION_BUILTIN:
        if (encoder->dither_cache) {
            *dither = encoder->dither_cache;
            status = SIXEL_OK;
        } else {
            status = sixel_prepare_builtin_palette(dither, encoder->builtin_palette);
        }
        goto end;
    case SIXEL_COLOR_OPTION_DEFAULT:
    default:
        break;
    }

    if (sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_PALETTE) {
        if (!sixel_frame_get_palette(frame)) {
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        status = sixel_dither_new(dither, sixel_frame_get_ncolors(frame),
                                  encoder->allocator);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        sixel_dither_set_palette(*dither, sixel_frame_get_palette(frame));
        sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
        if (sixel_frame_get_transparent(frame) != (-1)) {
            sixel_dither_set_transparent(*dither, sixel_frame_get_transparent(frame));
        }
        if (*dither && encoder->dither_cache) {
            sixel_dither_unref(encoder->dither_cache);
        }
        goto end;
    }

    if (sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_GRAYSCALE) {
        switch (sixel_frame_get_pixelformat(frame)) {
        case SIXEL_PIXELFORMAT_G1:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G1);
            break;
        case SIXEL_PIXELFORMAT_G2:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G2);
            break;
        case SIXEL_PIXELFORMAT_G4:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G4);
            break;
        case SIXEL_PIXELFORMAT_G8:
            *dither = sixel_dither_get(SIXEL_BUILTIN_G8);
            break;
        default:
            *dither = NULL;
            status = SIXEL_LOGIC_ERROR;
            goto end;
        }
        if (*dither && encoder->dither_cache) {
            sixel_dither_unref(encoder->dither_cache);
        }
        sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));
        status = SIXEL_OK;
        goto end;
    }

    if (encoder->dither_cache) {
        sixel_dither_unref(encoder->dither_cache);
    }
    status = sixel_dither_new(dither, encoder->reqcolors, encoder->allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_dither_initialize(*dither,
                                     sixel_frame_get_pixels(frame),
                                     sixel_frame_get_width(frame),
                                     sixel_frame_get_height(frame),
                                     sixel_frame_get_pixelformat(frame),
                                     encoder->method_for_largest,
                                     encoder->method_for_rep,
                                     encoder->quality_mode);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    histogram_colors = sixel_dither_get_num_of_histogram_colors(*dither);
    if (histogram_colors <= encoder->reqcolors) {
        encoder->method_for_diffuse = SIXEL_DIFFUSE_NONE;
    }
    sixel_dither_set_pixelformat(*dither, sixel_frame_get_pixelformat(frame));

    status = SIXEL_OK;

end:
    return status;
}


/* from libsixel-1.10.3 */
/* clip a frame with settings of specified encoder object */
static SIXELSTATUS
sixel_encoder_do_clip(
    sixel_encoder_t /* in */    *encoder,   /* encoder object */
    sixel_frame_t   /* in */    *frame)     /* frame object to be resized */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int src_width;
    int src_height;
    int clip_x;
    int clip_y;
    int clip_w;
    int clip_h;

    /* get frame width and height */
    src_width = sixel_frame_get_width(frame);
    src_height = sixel_frame_get_height(frame);

    /* settings around clipping */
    clip_x = encoder->clipx;
    clip_y = encoder->clipy;
    clip_w = encoder->clipwidth;
    clip_h = encoder->clipheight;

    /* adjust clipping width with comparing it to frame width */
    if (clip_w + clip_x > src_width) {
        if (clip_x > src_width) {
            clip_w = 0;
        } else {
            clip_w = src_width - clip_x;
        }
    }

    /* adjust clipping height with comparing it to frame height */
    if (clip_h + clip_y > src_height) {
        if (clip_y > src_height) {
            clip_h = 0;
        } else {
            clip_h = src_height - clip_y;
        }
    }

    /* do clipping */
    if (clip_w > 0 && clip_h > 0) {
        status = sixel_frame_clip(frame, clip_x, clip_y, clip_w, clip_h);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    /* success */
    status = SIXEL_OK;

end:
    return status;
}

/* from libsixel-1.10.3 */
/* resize a frame with settings of specified encoder object */
static SIXELSTATUS
sixel_encoder_do_resize(
    sixel_encoder_t /* in */    *encoder,   /* encoder object */
    sixel_frame_t   /* in */    *frame)     /* frame object to be resized */
{
    SIXELSTATUS status = SIXEL_FALSE;
    int src_width;
    int src_height;
    int dst_width;
    int dst_height;

    /* get frame width and height */
    src_width = sixel_frame_get_width(frame);
    src_height = sixel_frame_get_height(frame);

    /* settings around scaling */
    dst_width = encoder->pixelwidth;    /* may be -1 (default) */
    dst_height = encoder->pixelheight;  /* may be -1 (default) */

    /* if the encoder has percentwidth or percentheight property,
       convert them to pixelwidth / pixelheight */
    if (encoder->percentwidth > 0) {
        dst_width = src_width * encoder->percentwidth / 100;
    }
    if (encoder->percentheight > 0) {
        dst_height = src_height * encoder->percentheight / 100;
    }

    /* if only either width or height is set, set also the other
       to retain frame aspect ratio */
    if (encoder->pixelwidth > 0 && dst_height <= 0) {
        dst_height = src_height * encoder->pixelwidth / src_width;
    }
    if (encoder->pixelheight > 0 && dst_width <= 0) {
        dst_width = src_width * encoder->pixelheight / src_height;
    }

    /* do resize */
    if (dst_width > 0 && dst_height > 0) {
        status = sixel_frame_resize(frame, dst_width, dst_height,
                                    encoder->method_for_resampling);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    /* success */
    status = SIXEL_OK;

end:
    return status;
}


/* from libsixel-1.10.3 */
static SIXELSTATUS
sixel_encoder_encode_frame(
    sixel_encoder_t *encoder,
    sixel_frame_t   *frame,
    sixel_output_t  *output)
{
    SIXELSTATUS status = SIXEL_FALSE;
    sixel_dither_t *dither = NULL;
    //int height;
    //int is_animation = 0;
    //int nwrite;

    /* evaluate -w, -h, and -c option: crop/scale input source */
    if (encoder->clipfirst) {
        /* clipping */
        status = sixel_encoder_do_clip(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        /* scaling */
        status = sixel_encoder_do_resize(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    } else {
        /* scaling */
        status = sixel_encoder_do_resize(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }

        /* clipping */
        status = sixel_encoder_do_clip(encoder, frame);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
    }

    /* prepare dither context */
    status = sixel_encoder_prepare_palette(encoder, frame, &dither);
    if (status != SIXEL_OK) {
        goto end;
    }

    if (encoder->dither_cache != NULL) {
        encoder->dither_cache = dither;
        sixel_dither_ref(dither);
    }

#if 0
    /* evaluate -v option: print palette */
    if (encoder->verbose) {
        if ((sixel_frame_get_pixelformat(frame) & SIXEL_FORMATTYPE_PALETTE)) {
            sixel_debug_print_palette(dither);
        }
    }
#endif

    /* evaluate -d option: set method for diffusion */
    sixel_dither_set_diffusion_type(dither, encoder->method_for_diffuse);

    /* evaluate -C option: set complexion score */
    if (encoder->complexion > 1) {
        sixel_dither_set_complexion_score(dither, encoder->complexion);
    }

    if (output) {
        sixel_output_ref(output);
    } else {
#if 0
        /* create output context */
        if (encoder->fuse_macro || encoder->macro_number >= 0) {
            /* -u or -n option */
            status = sixel_output_new(&output,
                                      sixel_hex_write_callback,
                                      &encoder->outfd,
                                      encoder->allocator);
        } else {
            status = sixel_output_new(&output,
                                      sixel_write_callback,
                                      &encoder->outfd,
                                      encoder->allocator);
        }
        if (SIXEL_FAILED(status)) {
            goto end;
        }
#endif
    }

    sixel_output_set_8bit_availability(output, encoder->f8bit);
    sixel_output_set_gri_arg_limit(output, encoder->has_gri_arg_limit);
    sixel_output_set_palette_type(output, encoder->palette_type);
    sixel_output_set_penetrate_multiplexer(
        output, encoder->penetrate_multiplexer);
    sixel_output_set_encode_policy(output, encoder->encode_policy);
    sixel_output_set_ormode(output, encoder->ormode);

#if 0
    if (sixel_frame_get_multiframe(frame) && !encoder->fstatic) {
        if (sixel_frame_get_loop_no(frame) != 0 || sixel_frame_get_frame_no(frame) != 0) {
            is_animation = 1;
        }
        height = sixel_frame_get_height(frame);
        (void) sixel_tty_scroll(sixel_write_callback, encoder->outfd, height, is_animation);
    }
#endif

    if (encoder->cancel_flag && *encoder->cancel_flag) {
        status = SIXEL_INTERRUPTED;
        goto end;
    }

    /* output sixel: junction of multi-frame processing strategy */
    if (encoder->fuse_macro) {  /* -u option */
        /* use macro */
        status = sixel_encoder_output_with_macro(frame, dither, output, encoder);
    } else if (encoder->macro_number >= 0) { /* -n option */
        /* use macro */
        status = sixel_encoder_output_with_macro(frame, dither, output, encoder);
    } else {
        /* do not use macro */
        status = sixel_encoder_output_without_macro(frame, dither, output, encoder);
    }

#if 0
    if (encoder->cancel_flag && *encoder->cancel_flag) {
        nwrite = sixel_write_callback("\x18\033\\", 3, &encoder->outfd);
        if (nwrite < 0) {
            status = (SIXEL_LIBC_ERROR | (errno & 0xff));
            sixel_helper_set_additional_message(
                "load_image_callback: sixel_write_callback() failed.");
            goto end;
        }
        status = SIXEL_INTERRUPTED;
    }
#endif

    if (SIXEL_FAILED(status)) {
        goto end;
    }

end:
    if (output) {
        sixel_output_unref(output);
    }
    if (dither) {
        sixel_dither_unref(dither);
    }

    return status;
}


void
try_to_draw_images(struct terminal *term)
{
	struct image *im;

	if (!term->sixel) {
		return;
	}

	foreach (im, term->images) {
		struct string text;

		if (!init_string(&text)) {
			return;
		}
		add_cursor_move_to_string(&text, im->y + 1, im->x + 1);
		add_string_to_string(&text, &im->pixels);

		if (text.length) {
			if (term->master) want_draw();
			hard_write(term->fdout, text.source, text.length);
			if (term->master) done_draw();
		}
		done_string(&text);
	}
}

void
delete_image(struct image *im)
{
	del_from_list(im);
	done_string(&im->pixels);
	mem_free(im);
}

int
add_image_to_document(struct document *doc, struct string *pixels, int lineno, struct image **imagine)
{
	unsigned char *indexed_pixels = NULL;
	unsigned char *palette = NULL;
	sixel_decoder_t *decoder = NULL;
	sixel_frame_t *frame = NULL;
	int ncolors;
	int width;
	int height;
	int ile = 0;
	struct image *im = mem_calloc(1, sizeof(*im));
	SIXELSTATUS status;

	if (imagine) {
		*imagine = NULL;
	}

	if (!im) {
		return 0;
	}
	if (!init_string(&im->pixels)) {
		mem_free(im);
		return 0;
	}
#ifdef CONFIG_MEMCOUNT
	init_allocator();
	status = sixel_decoder_new(&decoder, el_sixel_allocator);
#else
	status = sixel_decoder_new(&decoder, NULL);
#endif
	if (SIXEL_FAILED(status)) {
		goto end;
	}
	status = sixel_decode_raw(
		(unsigned char *)pixels->source,
		pixels->length,
		&indexed_pixels,
		&width,
		&height,
		&palette,
		&ncolors,
		decoder->allocator
	);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	status = sixel_frame_new(&frame, decoder->allocator);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	status = sixel_frame_init(
		frame,
		indexed_pixels,
		width,
		height,
		SIXEL_PIXELFORMAT_PAL8,
		palette,
		ncolors
	);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	im->y = lineno;
	im->x = 0;
	im->width = width;
	im->height = height;
	add_string_to_string(&im->pixels, pixels);

	ile = (height + doc->options.cell_height - 1) / doc->options.cell_height;
	add_to_list(doc->images, im);
end:
	sixel_frame_unref(frame);
	sixel_decoder_unref(decoder);

	if (imagine) {
		*imagine = im;
	}

	return ile;
}

struct image *
copy_frame(struct image *src, struct el_box *box, int cell_width, int cell_height, int dx, int dy)
{
	sixel_decoder_t *decoder = NULL;
	sixel_encoder_t *encoder = NULL;
	sixel_output_t *output = NULL;
	sixel_frame_t *frame = NULL;
	unsigned char *indexed_pixels = NULL;
	unsigned char *palette = NULL;
	int ncolors;
	int width;
	int height;
	int x;
	int y;
	struct image *dest = mem_calloc(1, sizeof(*dest));
	SIXELSTATUS status;

	if (!dest) {
		return NULL;
	}

	if (!init_string(&dest->pixels)) {
		mem_free(dest);
		return NULL;
	}
#ifdef CONFIG_MEMCOUNT
	init_allocator();
	status = sixel_decoder_new(&decoder, el_sixel_allocator);
#else
	status = sixel_decoder_new(&decoder, NULL);
#endif

	if (SIXEL_FAILED(status)) {
		goto end;
	}

	status = sixel_decode_raw(
		(unsigned char *)src->pixels.source,
		src->pixels.length,
		&indexed_pixels,
		&width,
		&height,
		&palette,
		&ncolors,
		decoder->allocator
	);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	status = sixel_frame_new(&frame, decoder->allocator);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	status = sixel_frame_init(
		frame,
		indexed_pixels,
		width,
		height,
		SIXEL_PIXELFORMAT_PAL8,
		palette,
		ncolors
	);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	status = sixel_encoder_new(&encoder, decoder->allocator);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	x = src->x - dx;
	y = src->y - dy;

	encoder->clipx = x >= 0 ? 0 : (-x * cell_width);
	encoder->clipy = y >= 0 ? 0 : (-y * cell_height);
	encoder->clipwidth = box->width * cell_width;
	encoder->clipheight = box->height * cell_height;

	if (src->width < encoder->clipwidth) {
		encoder->clipwidth = src->width;
	}
	if (src->height < encoder->clipheight) {
		encoder->clipheight = src->height;
	}

	if (x * cell_width + encoder->clipwidth >= box->width * cell_width) {
		encoder->clipwidth = (box->width * cell_width - x * cell_width);
	}
	if (y * cell_height + encoder->clipheight >= box->height * cell_height) {
		encoder->clipheight = (box->height * cell_height - y * cell_height);
	}
	status = sixel_output_new(&output, sixel_write_callback, &dest->pixels, NULL);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	status = sixel_encoder_encode_frame(encoder, frame, output);

	if (SIXEL_FAILED(status)) {
		goto end;
	}
	dest->x = x < 0 ? 0 : x;
	dest->y = y < 0 ? 0 : y;
	dest->width = encoder->clipx >= src->width ? 0 : sixel_frame_get_width(frame);
	dest->height = encoder->clipy >= src->height ? 0 : sixel_frame_get_height(frame);
end:
	sixel_frame_unref(frame);
	sixel_output_unref(output);
	sixel_decoder_unref(decoder);
	sixel_encoder_unref(encoder);

	if (!dest->width || !dest->height) {
		done_string(&dest->pixels);
		mem_free(dest);
		return NULL;
	}

	return dest;
}

unsigned char *
el_sixel_get_image(char *data, int length, int *outlen)
{
	SIXELSTATUS status = SIXEL_FALSE;
	sixel_encoder_t *encoder = NULL;
	unsigned char *ret = NULL;
	*outlen = 0;

#ifdef HAVE_MEMFD_CREATE
#ifdef CONFIG_MEMCOUNT
	init_allocator();
	status = sixel_encoder_new(&encoder, el_sixel_allocator);
#else
	status = sixel_encoder_new(&encoder, NULL);
#endif

	if (SIXEL_FAILED(status)) {
		goto error;
	}
	int fdout = -1;

	encoder->outfd = memfd_create("out.sixel", 0);
	fdout = dup(encoder->outfd);
	encoder->fstatic = 1;

	int fdin = memfd_create("input.sixel", 0);
	FILE *f = fdopen(fdin, "wb");

	if (!f) {
		goto error;
	}
	fwrite(data, 1, length, f);
	rewind(f);

	struct string name;
	if (!init_string(&name)) {
		goto error;
	}
	add_format_to_string(&name, "/proc/self/fd/%d", fdin);
	status = sixel_encoder_encode(encoder, name.source);
	done_string(&name);

	if (SIXEL_FAILED(status)) {
		goto error;
	}

	struct stat sb;
	fstat(fdout, &sb);

	if (sb.st_size > 0) {
		ret = (unsigned char *)mem_alloc(sb.st_size);

		if (ret) {
			FILE *f2 = fdopen(fdout, "rb");
			if (f2) {
				rewind(f2);
				*outlen = (int)fread(ret, 1, (size_t)sb.st_size, f2);
				fclose(f2);
			}
		}
	}
	close(fdout);

error:
	if (f) {
		fclose(f);
	}
	sixel_encoder_unref(encoder);
#endif
	return ret;
}
