
#ifndef EL__DOCUMENT_CSS_SCANNER_H
#define EL__DOCUMENT_CSS_SCANNER_H

#include "util/scanner.h"

/** The various token types and what they contain. Patterns taken from
 * the flex scanner declarations in the CSS 2 Specification.
 *
 * Char tokens range from 1 to 255 and have their char value as type
 * meaning non char tokens have values from 256 and up.
 *
 * @verbatim
 * {...} means char group, <...> means token
 * {identstart}	[a-z_]|{nonascii}
 * {ident}	[a-z0-9_-]|{nonascii}
 * <ident>	{identstart}{ident}*
 * <name>	{ident}+
 * <number>	[0-9]+|[0-9]*"."[0-9]+
 * @endverbatim */
enum css_token_type {
	/* Doxygen 1.5.2 does not support member groups inside enum.  */

	/* Low level string tokens: */

	CSS_TOKEN_IDENT = 256,	/**< @<ident> */
	CSS_TOKEN_NUMBER,	/**< @<number> */
	/*! Percentage is put because although it looks like being composed
	 * of @<number> and '@%' floating point numbers are really not
	 * allowed but strtol() will round it down for us ;) */
	CSS_TOKEN_PERCENTAGE,	/**< @<number>% */
	CSS_TOKEN_STRING,	/**< Char sequence delimited by matching ' or " */

	/* High level string tokens: */

	/* The various number values; dimension being the most generic */
	CSS_TOKEN_ANGLE,	/**< @<number>rad, @<number>grad or @<number>deg */
	CSS_TOKEN_DIMENSION,	/**< @<number>@<ident> */
	CSS_TOKEN_EM,		/**< @<number>em */
	CSS_TOKEN_EX,		/**< @<number>ex */
	CSS_TOKEN_FREQUENCY,	/**< @<number>Hz or @<number>kHz */
	CSS_TOKEN_LENGTH,	/**< @<number>{px,cm,mm,in,pt,pc} */
	CSS_TOKEN_TIME,		/**< @<number>ms or @<number>s */

	/*! XXX: @c CSS_TOKEN_HASH conflicts with #CSS_TOKEN_HEX_COLOR.
	 * Generating hex color tokens has precedence and the hash token
	 * user have to treat @c CSS_TOKEN_HASH and #CSS_TOKEN_HEX_COLOR
	 * alike. */
	CSS_TOKEN_HASH,		/**< #@<name> */
	CSS_TOKEN_HEX_COLOR,	/**< #[0-9a-f]@\{3,6} */

	/*! For all unknown functions we generate on token contain both
	 * function name and args so scanning/parsing is easier. Besides
	 * we already check for ending ')'.  */
	CSS_TOKEN_FUNCTION,	/**< @<ident>(@<args>) */
	/*! For known functions where we need several args [like rgb()]
	 * we want to generate tokens for every arg and arg delimiter
	 * ( ',' or ')' ). */
	CSS_TOKEN_RGB,		/**< rgb( */
	/*! Because url() is a bit triggy: it can contain both @<string>
	 * and some chars that would other wise make the scanner probably
	 * choke we also include the arg in that token. Besides it will
	 * make things like 'background' property parsing easier. */
	CSS_TOKEN_URL,		/**< url(@<arg>) */

	/* @-rule symbols */

	CSS_TOKEN_AT_KEYWORD,	/**< @@@<ident> */
	CSS_TOKEN_AT_CHARSET,	/**< @@charset */
	CSS_TOKEN_AT_FONT_FACE,	/**< @@font-face */
	CSS_TOKEN_AT_IMPORT,	/**< @@import */
	CSS_TOKEN_AT_MEDIA,	/**< @@media */
	CSS_TOKEN_AT_PAGE,	/**< @@page */

	CSS_TOKEN_IMPORTANT,	/**< !@<whitespace>important */

	/* TODO: Selector stuff: */

	CSS_TOKEN_SELECT_SPACE_LIST,	/**< ~= */
	CSS_TOKEN_SELECT_HYPHEN_LIST,	/**< |= */
	CSS_TOKEN_SELECT_BEGIN,		/**< ^= */
	CSS_TOKEN_SELECT_END,		/**< $= */
	CSS_TOKEN_SELECT_CONTAINS,	/**< *= */

	/* Special tokens: */

	/** A special token for unrecognized strings */
	CSS_TOKEN_GARBAGE,

	/** Token type used internally when scanning to signal that the token
	 * should not be recorded in the scanners token table. */
	CSS_TOKEN_SKIP,

	/** Another internal token type used both to mark unused tokens in the
	 * scanner table as invalid or when scanning to signal that the
	 * scanning should end. */
	CSS_TOKEN_NONE = 0,
};

extern struct scanner_info css_scanner_info;

#define skip_css_tokens(scanner, type) \
	skip_scanner_tokens(scanner, type, get_css_precedence(type))

#define get_css_precedence(token_type) \
	((token_type) == '}' ? (1 << 10) : \
	 (token_type) == '{' ? (1 <<  9) : \
	 (token_type) == ';' ? (1 <<  8) : \
	 (token_type) == ')' ? (1 <<  7) : 0)

/** Check whether it is safe to skip the @a type when looking for @a skipto. */
static inline int
check_css_precedence(int type, int skipto)
{
	return get_css_precedence(type) < get_css_precedence(skipto);
}

#endif
