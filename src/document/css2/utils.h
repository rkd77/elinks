/*
 * Copyright 2009 John-Mark Bell <jmb@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NETSURF_CSS_UTILS_H_
#define NETSURF_CSS_UTILS_H_

#include <libcss/libcss.h>

#include "netsurf/css.h"

/** DPI of the screen, in fixed point units */
extern css_fixed nscss_screen_dpi;

/** Medium screen density for device viewing distance. */
extern css_fixed nscss_baseline_pixel_density;

/**
 * Length conversion context data.
 */
typedef struct nscss_len_ctx {
	/**
	 * Viewport width in px.
	 * Only used if unit is vh, vw, vi, vb, vmin, or vmax.
	 */
	int vw;
	/**
	 * Viewport height in px.
	 * Only used if unit is vh, vw, vi, vb, vmin, or vmax.
	 */
	int vh;
	/**
	 * Computed style for the document root element.
	 * May be NULL if unit is not rem, or rlh.
	 */
	const css_computed_style *root_style;
} nscss_len_ctx;

/**
 * Convert an absolute CSS length to points.
 *
 * \param[in] ctx     Length conversion context.
 * \param[in] length  Absolute CSS length.
 * \param[in] unit    Unit of the length.
 * \return length in points
 */
css_fixed nscss_len2pt(
		const nscss_len_ctx *ctx,
		css_fixed length,
		css_unit unit);

/**
 * Convert a CSS length to pixels.
 *
 * \param[in] ctx     Length conversion context.
 * \param[in] length  Length to convert.
 * \param[in] unit    Corresponding unit.
 * \param[in] style   Computed style applying to length.
 *                    May be NULL if unit is not em, ex, cap, ch, or ic.
 * \return length in pixels
 */
css_fixed nscss_len2px(
		const nscss_len_ctx *ctx,
		css_fixed length,
		css_unit unit,
		const css_computed_style *style);

/**
 * Convert css pixels to physical pixels.
 *
 * \param[in] css_pixels  Length in css pixels.
 * \return length in physical pixels
 */
static inline css_fixed nscss_pixels_css_to_physical(
		css_fixed css_pixels)
{
	return FDIV(FMUL(css_pixels, nscss_screen_dpi),
			nscss_baseline_pixel_density);
}

/**
 * Convert physical pixels to css pixels.
 *
 * \param[in] physical_pixels  Length in physical pixels.
 * \return length in css pixels
 */
static inline css_fixed nscss_pixels_physical_to_css(
		css_fixed physical_pixels)
{
	return FDIV(FMUL(physical_pixels, nscss_baseline_pixel_density),
			nscss_screen_dpi);
}

/**
 * Temporary helper wrappers for for libcss computed style getter, while
 * we don't support flexbox related property values.
 */

static inline uint8_t ns_computed_display(
		const css_computed_style *style, bool root)
{
	uint8_t value = css_computed_display(style, root);

	if (value == CSS_DISPLAY_FLEX) {
		return CSS_DISPLAY_BLOCK;

	} else if (value == CSS_DISPLAY_INLINE_FLEX) {
		return CSS_DISPLAY_INLINE_BLOCK;
	}

	return value;
}


static inline uint8_t ns_computed_display_static(
		const css_computed_style *style)
{
	uint8_t value = css_computed_display_static(style);

	if (value == CSS_DISPLAY_FLEX) {
		return CSS_DISPLAY_BLOCK;

	} else if (value == CSS_DISPLAY_INLINE_FLEX) {
		return CSS_DISPLAY_INLINE_BLOCK;
	}

	return value;
}


static inline uint8_t ns_computed_min_height(
		const css_computed_style *style,
		css_fixed *length, css_unit *unit)
{
	uint8_t value = css_computed_min_height(style, length, unit);

	if (value == CSS_MIN_HEIGHT_AUTO) {
		value = CSS_MIN_HEIGHT_SET;
		*length = 0;
		*unit = CSS_UNIT_PX;
	}

	return value;
}


static inline uint8_t ns_computed_min_width(
		const css_computed_style *style,
		css_fixed *length, css_unit *unit)
{
	uint8_t value = css_computed_min_width(style, length, unit);

	if (value == CSS_MIN_WIDTH_AUTO) {
		value = CSS_MIN_WIDTH_SET;
		*length = 0;
		*unit = CSS_UNIT_PX;
	}

	return value;
}

#endif
