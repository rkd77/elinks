/** CSS property value parser
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/property.h"
#include "document/css/scanner.h"
#include "document/css/value.h"
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


int
css_parse_color_value(struct css_property_info *propinfo,
		      union css_property_value *value,
		      struct scanner *scanner)
{
	struct scanner_token *token = get_scanner_token(scanner);

	assert(propinfo->value_type == CSS_VT_COLOR);

	if (token->type == CSS_TOKEN_RGB) {
		/* RGB function */
		int shift;

		token = get_next_scanner_token(scanner);

		/* First color component is shifted 16, next is shifted 8 and
		 * last is not shifted. */
		for (shift = 16; token && shift >= 0; shift -= 8) {
			/* The first two args are terminated by ',' and the
			 * last one by ')'. */
			unsigned char paskynator = shift ? ',' : ')';
			const unsigned char *nstring = token->string;
			int part;

			/* Are the current and next token valid? */
			if ((token->type != CSS_TOKEN_NUMBER
			     && token->type != CSS_TOKEN_PERCENTAGE)
			    || !check_next_scanner_token(scanner, paskynator))
				return 0;

			/* Parse the digit */
			part = strtol(token->string, (char **) &nstring, 10);
			if (token->string == nstring)
				return 0;

			/* Adjust percentage values */
			if (token->type == CSS_TOKEN_PERCENTAGE) {
				int_bounds(&part, 0, 100);
				part *= 255;
				part /= 100;
			}

			/* Adjust color component value and add it */
			int_bounds(&part, 0, 255);
			value->color |= part << shift;

			/* Paskynate the token arg and separator */
			token = skip_css_tokens(scanner, paskynator);
		}

		return 1;
	}

	/* Just a color value we already know how to parse. */
	if (token->type != CSS_TOKEN_IDENT
	    && token->type != CSS_TOKEN_HEX_COLOR)
		return 0;

	if (decode_color(token->string, token->length, &value->color) < 0) {
		return 0;
	}

	skip_css_tokens(scanner, token->type);
	return 1;
}

int
css_parse_background_value(struct css_property_info *propinfo,
			   union css_property_value *value,
			   struct scanner *scanner)
{
	int success = 0;

	assert(propinfo->value_type == CSS_VT_COLOR);

	/* This is pretty naive, we just jump space by space, trying to parse
	 * each token as a color. */

	while (scanner_has_tokens(scanner)) {
		struct scanner_token *token = get_scanner_token(scanner);

		if (!check_css_precedence(token->type, ';'))
			break;

		if (token->type == ','
		    || !css_parse_color_value(propinfo, value, scanner)) {
			skip_scanner_token(scanner);
			continue;
		}

		success++;
	}

	return success;
}


int
css_parse_font_style_value(struct css_property_info *propinfo,
			   union css_property_value *value,
			   struct scanner *scanner)
{
	struct scanner_token *token = get_scanner_token(scanner);

	assert(propinfo->value_type == CSS_VT_FONT_ATTRIBUTE);

	if (token->type != CSS_TOKEN_IDENT) return 0;

	if (scanner_token_contains(token, "italic")
	    || scanner_token_contains(token, "oblique")) {
		value->font_attribute.add |= AT_ITALIC;

	} else if (scanner_token_contains(token, "underline")) {
		value->font_attribute.add |= AT_UNDERLINE;

	} else if (scanner_token_contains(token, "normal")) {
		value->font_attribute.rem |= AT_ITALIC;

	} else {
		return 0;
	}

	skip_css_tokens(scanner, CSS_TOKEN_IDENT);
	return 1;
}


int
css_parse_font_weight_value(struct css_property_info *propinfo,
			    union css_property_value *value,
			    struct scanner *scanner)
{
	struct scanner_token *token = get_scanner_token(scanner);
	unsigned char *nstring;
	int weight;

	assert(propinfo->value_type == CSS_VT_FONT_ATTRIBUTE);

	if (token->type == CSS_TOKEN_IDENT) {
		if (scanner_token_contains(token, "bolder")) {
			value->font_attribute.add |= AT_BOLD;

		} else if (scanner_token_contains(token, "lighter")) {
			value->font_attribute.rem |= AT_BOLD;

		} else if (scanner_token_contains(token, "bold")) {
			value->font_attribute.add |= AT_BOLD;

		} else if (scanner_token_contains(token, "normal")) {
			value->font_attribute.rem |= AT_BOLD;

		} else {
			return 0;
		}

		skip_css_tokens(scanner, CSS_TOKEN_IDENT);
		return 1;
	}

	if (token->type != CSS_TOKEN_NUMBER) return 0;

	/* TODO: Comma separated list of weights?! */
	weight = strtol(token->string, (char **) &nstring, 10);
	if (token->string == nstring) return 0;

	skip_css_tokens(scanner, CSS_TOKEN_NUMBER);
	/* The font weight(s) have values between 100 to 900.  These
	 * values form an ordered sequence, where each number indicates
	 * a weight that is at least as dark as its predecessor.
	 *
	 * normal -> Same as '400'.  bold Same as '700'.
	 */
	int_bounds(&weight, 100, 900);
	if (weight >= 700) value->font_attribute.add |= AT_BOLD;
	return 1;
}


int
css_parse_list_style_value(struct css_property_info *propinfo,
			   union css_property_value *value,
			   struct scanner *scanner)
{
	struct scanner_token *token = get_scanner_token(scanner);

	assert(propinfo->value_type == CSS_VT_LIST_STYLE);

	if (token->type != CSS_TOKEN_IDENT) return 0;

	if (scanner_token_contains(token, "none")) {
		value->list_style = CSS_LIST_NONE;

	} else if (scanner_token_contains(token, "disc")) {
		value->list_style = CSS_LIST_DISC;

	} else if (scanner_token_contains(token, "circle")) {
		value->list_style = CSS_LIST_CIRCLE;

	} else if (scanner_token_contains(token, "square")) {
		value->list_style = CSS_LIST_SQUARE;

	} else if (scanner_token_contains(token, "decimal")) {
		value->list_style = CSS_LIST_DECIMAL;

	} else if (scanner_token_contains(token, "decimal-leading-zero")) {
		value->list_style = CSS_LIST_DECIMAL_LEADING_ZERO;

	} else if (scanner_token_contains(token, "lower-roman")) {
		value->list_style = CSS_LIST_LOWER_ROMAN;

	} else if (scanner_token_contains(token, "upper-roman")) {
		value->list_style = CSS_LIST_UPPER_ROMAN;

	} else if (scanner_token_contains(token, "lower-alpha")) {
		value->list_style = CSS_LIST_LOWER_ALPHA;

	} else if (scanner_token_contains(token, "upper-alpha")) {
		value->list_style = CSS_LIST_UPPER_ALPHA;

	} else if (scanner_token_contains(token, "lower-greek")) {
		value->list_style = CSS_LIST_LOWER_GREEK;

	} else if (scanner_token_contains(token, "lower-latin")) {
		value->list_style = CSS_LIST_LOWER_LATIN;

	} else if (scanner_token_contains(token, "upper-latin")) {
		value->list_style = CSS_LIST_UPPER_LATIN;

	} else if (scanner_token_contains(token, "hebrew")) {
		value->list_style = CSS_LIST_HEBREW;

	} else if (scanner_token_contains(token, "armenian")) {
		value->list_style = CSS_LIST_ARMENIAN;

	} else if (scanner_token_contains(token, "georgian")) {
		value->list_style = CSS_LIST_GEORGIAN;

	} else if (scanner_token_contains(token, "cjk-ideographic")) {
		value->list_style = CSS_LIST_CJK_IDEOGRAPHIC;

	} else if (scanner_token_contains(token, "hiragana")) {
		value->list_style = CSS_LIST_HIRAGANA;

	} else if (scanner_token_contains(token, "katakana")) {
		value->list_style = CSS_LIST_KATAKANA;

	} else if (scanner_token_contains(token, "hiragana-iroha")) {
		value->list_style = CSS_LIST_HIRAGANA_IROHA;

	} else if (scanner_token_contains(token, "katakana-iroha")) {
		value->list_style = CSS_LIST_KATAKANA_IROHA;

	} else {
		return 0;
	}

	skip_css_tokens(scanner, CSS_TOKEN_IDENT);
	return 1;
}

int
css_parse_text_align_value(struct css_property_info *propinfo,
			   union css_property_value *value,
			   struct scanner *scanner)
{
	struct scanner_token *token = get_scanner_token(scanner);

	assert(propinfo->value_type == CSS_VT_TEXT_ALIGN);

	if (token->type != CSS_TOKEN_IDENT) return 0;

	if (scanner_token_contains(token, "left")) {
		value->text_align = ALIGN_LEFT;

	} else 	if (scanner_token_contains(token, "right")) {
		value->text_align = ALIGN_RIGHT;

	} else 	if (scanner_token_contains(token, "center")) {
		value->text_align = ALIGN_CENTER;

	} else 	if (scanner_token_contains(token, "justify")) {
		value->text_align = ALIGN_JUSTIFY;

	} else {
		return 0;
	}

	skip_css_tokens(scanner, CSS_TOKEN_IDENT);
	return 1;
}


int
css_parse_text_decoration_value(struct css_property_info *propinfo,
				union css_property_value *value,
				struct scanner *scanner)
{
	struct scanner_token *token = get_scanner_token(scanner);

	assert(propinfo->value_type == CSS_VT_FONT_ATTRIBUTE);

	if (token->type != CSS_TOKEN_IDENT) return 0;

	/* TODO: It is possible to have multiple values here,
	 * 'background'-style. --pasky */
	if (scanner_token_contains(token, "underline")) {
		value->font_attribute.add |= AT_UNDERLINE;

	} else if (scanner_token_contains(token, "none")) {
		value->font_attribute.rem |= AT_UNDERLINE;

	} else {
		return 0;
	}

	skip_css_tokens(scanner, CSS_TOKEN_IDENT);
	return 1;
}

int
css_parse_white_space_value(struct css_property_info *propinfo,
			    union css_property_value *value,
			    struct scanner *scanner)
{
	struct scanner_token *token = get_scanner_token(scanner);

	assert(propinfo->value_type == CSS_VT_FONT_ATTRIBUTE);

	if (token->type != CSS_TOKEN_IDENT) return 0;

	/* FIXME: nowrap */
	if (scanner_token_contains(token, "pre")) {
		value->font_attribute.add |= AT_PREFORMATTED;

	} else if (scanner_token_contains(token, "normal")) {
		value->font_attribute.rem |= AT_PREFORMATTED;

	} else {
		return 0;
	}

	skip_css_tokens(scanner, CSS_TOKEN_IDENT);
	return 1;
}

int
css_parse_display_value(struct css_property_info *propinfo,
			union css_property_value *value,
			struct scanner *scanner)
{
	struct scanner_token *token = get_scanner_token(scanner);

	assert(propinfo->value_type == CSS_VT_DISPLAY);

	if (token->type != CSS_TOKEN_IDENT) return 0;

	/* FIXME: This is _very_ simplistic */
	if (scanner_token_contains(token, "inline")) {
		value->display = CSS_DISP_INLINE;
	} else if (scanner_token_contains(token, "inline-block")) {
		value->display = CSS_DISP_INLINE; /* XXX */
	} else if (scanner_token_contains(token, "block")) {
		value->display = CSS_DISP_BLOCK;
	} else if (scanner_token_contains(token, "none")) {
		value->display = CSS_DISP_NONE;
	} else {
		return 0;
	}

	skip_css_tokens(scanner, CSS_TOKEN_IDENT);
	return 1;
}


int
css_parse_value(struct css_property_info *propinfo,
		union css_property_value *value,
		struct scanner *scanner)
{
	struct scanner_token *token;

	assert(scanner && value && propinfo);
	assert(propinfo->parser);

	/* Check that we actually have some token to pass on */
	token = get_scanner_token(scanner);
	if (!token) return 0;

	return propinfo->parser(propinfo, value, scanner);
}
