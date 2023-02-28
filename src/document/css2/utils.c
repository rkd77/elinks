/*
 * Copyright 2004 James Bursa <james@netsurf-browser.org>
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

#include <assert.h>

#include "utils/nsoption.h"
#include "utils/log.h"

#include "css/utils.h"

/** Screen DPI in fixed point units: defaults to 90, which RISC OS uses */
css_fixed nscss_screen_dpi = F_90;

/** Medium screen density for device viewing distance. */
css_fixed nscss_baseline_pixel_density = F_96;

/**
 * Map viewport-relative length units to either vh or vw.
 *
 * Non-viewport-relative units are unchanged.
 *
 * \param[in] ctx   Length conversion context.
 * \param[in] unit  Unit to map.
 * \return the mapped unit.
 */
static inline css_unit css_utils__fudge_viewport_units(
		const nscss_len_ctx *ctx,
		css_unit unit)
{
	switch (unit) {
	case CSS_UNIT_VI:
		assert(ctx->root_style != NULL);
		if (css_computed_writing_mode(ctx->root_style) ==
				CSS_WRITING_MODE_HORIZONTAL_TB) {
			unit = CSS_UNIT_VW;
		} else {
			unit = CSS_UNIT_VH;
		}
		break;
	case CSS_UNIT_VB:
		assert(ctx->root_style != NULL);
		if (css_computed_writing_mode(ctx->root_style) ==
				CSS_WRITING_MODE_HORIZONTAL_TB) {
			unit = CSS_UNIT_VH;
		} else {
			unit = CSS_UNIT_VW;
		}
		break;
	case CSS_UNIT_VMIN:
		if (ctx->vh < ctx->vw) {
			unit = CSS_UNIT_VH;
		} else {
			unit = CSS_UNIT_VW;
		}
		break;
	case CSS_UNIT_VMAX:
		if (ctx->vh > ctx->vw) {
			unit = CSS_UNIT_VH;
		} else {
			unit = CSS_UNIT_VW;
		}
		break;
	default: break;
	}

	return unit;
}

/* exported interface documented in content/handlers/css/utils.h */
css_fixed nscss_len2pt(
		const nscss_len_ctx *ctx,
		css_fixed length,
		css_unit unit)
{
	/* Length must not be relative */
	assert(unit != CSS_UNIT_EM &&
			unit != CSS_UNIT_EX &&
			unit != CSS_UNIT_CAP &&
			unit != CSS_UNIT_CH &&
			unit != CSS_UNIT_IC &&
			unit != CSS_UNIT_REM &&
			unit != CSS_UNIT_RLH);

	unit = css_utils__fudge_viewport_units(ctx, unit);

	switch (unit) {
	/* We assume the screen and any other output has the same dpi */
	/* 1in = DPIpx => 1px = (72/DPI)pt */
	case CSS_UNIT_PX: return FDIV(FMUL(length, F_72), F_96);
	/* 1in = 72pt */
	case CSS_UNIT_IN: return FMUL(length, F_72);
	/* 1in = 2.54cm => 1cm = (72/2.54)pt */
	case CSS_UNIT_CM: return FMUL(length,
				FDIV(F_72, FLTTOFIX(2.54)));
	/* 1in = 25.4mm => 1mm = (72/25.4)pt */
	case CSS_UNIT_MM: return FMUL(length,
				      FDIV(F_72, FLTTOFIX(25.4)));
	/* 1in = 101.6q => 1mm = (72/101.6)pt */
	case CSS_UNIT_Q: return FMUL(length,
				      FDIV(F_72, FLTTOFIX(101.6)));
	case CSS_UNIT_PT: return length;
	/* 1pc = 12pt */
	case CSS_UNIT_PC: return FMUL(length, INTTOFIX(12));
	case CSS_UNIT_VH: return FDIV(FMUL(FDIV(FMUL(length, ctx->vh), F_100), F_72), F_96);
	case CSS_UNIT_VW: return FDIV(FMUL(FDIV(FMUL(length,ctx->vw), F_100), F_72), F_96);
	default: break;
	}

	return 0;
}

/* exported interface documented in content/handlers/css/utils.h */
css_fixed nscss_len2px(
		const nscss_len_ctx *ctx,
		css_fixed length,
		css_unit unit,
		const css_computed_style *style)
{
	/* We assume the screen and any other output has the same dpi */
	css_fixed px_per_unit;

	unit = css_utils__fudge_viewport_units(ctx, unit);

	switch (unit) {
	case CSS_UNIT_EM:
	case CSS_UNIT_EX:
	case CSS_UNIT_CAP:
	case CSS_UNIT_CH:
	case CSS_UNIT_IC:
	{
		css_fixed font_size = 0;
		css_unit font_unit = CSS_UNIT_PT;

		assert(style != NULL);

		css_computed_font_size(style, &font_size, &font_unit);

		/* Convert to points */
		font_size = nscss_len2pt(ctx, font_size, font_unit);

		/* Clamp to configured minimum */
		if (font_size < FDIV(INTTOFIX(nsoption_int(font_min_size)), F_10)) {
		  font_size = FDIV(INTTOFIX(nsoption_int(font_min_size)), F_10);
		}

		/* Convert to pixels (manually, to maximise precision)
		 * 1in = 72pt => 1pt = (DPI/72)px */
		px_per_unit = FDIV(FMUL(font_size, F_96), F_72);

		/* Scale non-em units to em.  We have fixed ratios. */
		switch (unit) {
		case CSS_UNIT_EX:
			px_per_unit = FMUL(px_per_unit, FLTTOFIX(0.6));
			break;
		case CSS_UNIT_CAP:
			px_per_unit = FMUL(px_per_unit, FLTTOFIX(0.9));
			break;
		case CSS_UNIT_CH:
			px_per_unit = FMUL(px_per_unit, FLTTOFIX(0.4));
			break;
		case CSS_UNIT_IC:
			px_per_unit = FMUL(px_per_unit, FLTTOFIX(1.1));
			break;
		default: break;
		}
	}
		break;
	case CSS_UNIT_PX:
		px_per_unit = F_1;
		break;
	/* 1in = 96 CSS pixels */
	case CSS_UNIT_IN:
		px_per_unit = F_96;
		break;
	/* 1in = 2.54cm => 1cm = (DPI/2.54)px */
	case CSS_UNIT_CM:
		px_per_unit = FDIV(F_96, FLTTOFIX(2.54));
		break;
	/* 1in = 25.4mm => 1mm = (DPI/25.4)px */
	case CSS_UNIT_MM:
		px_per_unit = FDIV(F_96, FLTTOFIX(25.4));
		break;
	/* 1in = 101.6q => 1q = (DPI/101.6)px */
	case CSS_UNIT_Q:
		px_per_unit = FDIV(F_96, FLTTOFIX(101.6));
		break;
	/* 1in = 72pt => 1pt = (DPI/72)px */
	case CSS_UNIT_PT:
		px_per_unit = FDIV(F_96, F_72);
		break;
	/* 1pc = 12pt => 1in = 6pc => 1pc = (DPI/6)px */
	case CSS_UNIT_PC:
		px_per_unit = FDIV(F_96, INTTOFIX(6));
		break;
	case CSS_UNIT_REM:
	{
		css_fixed font_size = 0;
		css_unit font_unit = CSS_UNIT_PT;

		assert(ctx->root_style != NULL);

		css_computed_font_size(ctx->root_style,
				&font_size, &font_unit);

		/* Convert to points */
		font_size = nscss_len2pt(ctx, font_size, font_unit);

		/* Clamp to configured minimum */
		if (font_size < FDIV(INTTOFIX(nsoption_int(font_min_size)), F_10)) {
			font_size = FDIV(INTTOFIX(nsoption_int(font_min_size)), F_10);
		}

		/* Convert to pixels (manually, to maximise precision)
		 * 1in = 72pt => 1pt = (DPI/72)px */
		px_per_unit = FDIV(FMUL(font_size, F_96), F_72);
		break;
	}
	/* 1rlh = <user_font_size>pt => 1rlh = (DPI/user_font_size)px */
	case CSS_UNIT_RLH:
		px_per_unit = FDIV(F_96, FDIV(
				INTTOFIX(nsoption_int(font_size)),
				INTTOFIX(10)));
		break;
	case CSS_UNIT_VH:
		px_per_unit = FDIV(ctx->vh, F_100);
		break;
	case CSS_UNIT_VW:
		px_per_unit = FDIV(ctx->vw, F_100);
		break;
	default:
		px_per_unit = 0;
		break;
	}

	px_per_unit = nscss_pixels_css_to_physical(px_per_unit);

	/* Ensure we round px_per_unit to the nearest whole number of pixels:
	 * the use of FIXTOINT() below will truncate. */
	px_per_unit += F_0_5;

	/* Calculate total number of pixels */
	return FMUL(length, TRUNCATEFIX(px_per_unit));
}
