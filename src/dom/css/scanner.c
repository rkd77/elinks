/* CSS token scanner utilities */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "dom/css/scanner.h"
#include "dom/scanner.h"
#include "dom/string.h"
#include "util/error.h"


/* Bitmap entries for the CSS character groups used in the scanner table */

enum css_char_group {
	CSS_CHAR_ALPHA		= (1 << 0),
	CSS_CHAR_DIGIT		= (1 << 1),
	CSS_CHAR_HEX_DIGIT	= (1 << 2),
	CSS_CHAR_IDENT		= (1 << 3),
	CSS_CHAR_IDENT_START	= (1 << 4),
	CSS_CHAR_NEWLINE	= (1 << 5),
	CSS_CHAR_NON_ASCII	= (1 << 6),
	CSS_CHAR_SGML_MARKUP	= (1 << 7),
	CSS_CHAR_TOKEN		= (1 << 8),
	CSS_CHAR_TOKEN_START	= (1 << 9),
	CSS_CHAR_WHITESPACE	= (1 << 10),
};

static const struct dom_scan_table_info css_scan_table_info[] = {
	DOM_SCAN_TABLE_RANGE("0", '9', CSS_CHAR_DIGIT | CSS_CHAR_HEX_DIGIT | CSS_CHAR_IDENT),
	DOM_SCAN_TABLE_RANGE("A", 'F', CSS_CHAR_HEX_DIGIT),
	DOM_SCAN_TABLE_RANGE("A", 'Z', CSS_CHAR_ALPHA | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),
	DOM_SCAN_TABLE_RANGE("a", 'f', CSS_CHAR_HEX_DIGIT),
	DOM_SCAN_TABLE_RANGE("a", 'z', CSS_CHAR_ALPHA | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),
	/* For the octal number impared (me including) \241 is 161 --jonas */
	DOM_SCAN_TABLE_RANGE("\241", 255, CSS_CHAR_NON_ASCII | CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),

	DOM_SCAN_TABLE_STRING(" \f\n\r\t\v\000", CSS_CHAR_WHITESPACE),
	DOM_SCAN_TABLE_STRING("\f\n\r",	 CSS_CHAR_NEWLINE),
	DOM_SCAN_TABLE_STRING("-",		 CSS_CHAR_IDENT),
	DOM_SCAN_TABLE_STRING(".#@!\"'<-/|^$*",	 CSS_CHAR_TOKEN_START),
	/* Unicode escape (that we do not handle yet) + other special chars */
	DOM_SCAN_TABLE_STRING("\\_",	 CSS_CHAR_IDENT | CSS_CHAR_IDENT_START),
	/* This should contain mostly used char tokens like ':' and maybe a few
	 * garbage chars that people might put in their CSS code */
	DOM_SCAN_TABLE_STRING("[({})];:,.>+~",	 CSS_CHAR_TOKEN),
	DOM_SCAN_TABLE_STRING("<![CDATA]->", CSS_CHAR_SGML_MARKUP),

	DOM_SCAN_TABLE_END,
};

#define CSS_STRING_MAP(str, type, family) \
	{ STATIC_DOM_STRING(str), CSS_TOKEN_##type, CSS_TOKEN_##family }

static const struct dom_scanner_string_mapping css_string_mappings[] = {
	CSS_STRING_MAP("Hz",		FREQUENCY,	DIMENSION),
	CSS_STRING_MAP("cm",		LENGTH,		DIMENSION),
	CSS_STRING_MAP("deg",		ANGLE,		DIMENSION),
	CSS_STRING_MAP("em",		EM,		DIMENSION),
	CSS_STRING_MAP("ex",		EX,		DIMENSION),
	CSS_STRING_MAP("grad",		ANGLE,		DIMENSION),
	CSS_STRING_MAP("in",		LENGTH,		DIMENSION),
	CSS_STRING_MAP("kHz",		FREQUENCY,	DIMENSION),
	CSS_STRING_MAP("mm",		LENGTH,		DIMENSION),
	CSS_STRING_MAP("ms",		TIME,		DIMENSION),
	CSS_STRING_MAP("pc",		LENGTH,		DIMENSION),
	CSS_STRING_MAP("pt",		LENGTH,		DIMENSION),
	CSS_STRING_MAP("px",		LENGTH,		DIMENSION),
	CSS_STRING_MAP("rad",		ANGLE,		DIMENSION),
	CSS_STRING_MAP("s",		TIME,		DIMENSION),

	CSS_STRING_MAP("rgb",		RGB,		FUNCTION),
	CSS_STRING_MAP("url",		URL,		FUNCTION),

	CSS_STRING_MAP("charset",	AT_CHARSET,	AT_KEYWORD),
	CSS_STRING_MAP("font-face",	AT_FONT_FACE,	AT_KEYWORD),
	CSS_STRING_MAP("import",	AT_IMPORT,	AT_KEYWORD),
	CSS_STRING_MAP("media",		AT_MEDIA,	AT_KEYWORD),
	CSS_STRING_MAP("page",		AT_PAGE,	AT_KEYWORD),

	DOM_STRING_MAP_END,
};

static struct dom_scanner_token *scan_css_tokens(struct dom_scanner *scanner);

struct dom_scanner_info dom_css_scanner_info = {
	css_string_mappings,
	css_scan_table_info,
	scan_css_tokens,
};

#define	check_css_table(c, bit)	(dom_css_scanner_info.scan_table[(c)] & (bit))

#define	scan_css(scanner, s, bit)					\
	while ((s) < (scanner)->end && check_css_table(*(s), bit)) (s)++;

#define	scan_back_css(scanner, s, bit)					\
	while ((s) >= (scanner)->string && check_css_table(*(s), bit)) (s)--;

#define	is_css_ident_start(c)	check_css_table(c, CSS_CHAR_IDENT_START)
#define	is_css_ident(c)		check_css_table(c, CSS_CHAR_IDENT)
#define	is_css_digit(c)		check_css_table(c, CSS_CHAR_DIGIT)
#define	is_css_hexdigit(c)	check_css_table(c, CSS_CHAR_HEX_DIGIT)
#define	is_css_char_token(c)	check_css_table(c, CSS_CHAR_TOKEN)
#define	is_css_token_start(c)	check_css_table(c, CSS_CHAR_TOKEN_START)


#define	skip_css(scanner, s, skipto)					\
	while (s < (scanner)->end					\
	       && *(s) != (skipto)					\
	       && check_css_precedence(*(s), skipto)) {			\
		if (isquote(*(s))) {					\
			int size = (scanner)->end - (s);		\
			unsigned char *end = memchr(s + 1, *(s), size);	\
									\
			if (end) (s) = end;				\
		}							\
		(s)++;							\
	}


static inline void
scan_css_token(struct dom_scanner *scanner, struct dom_scanner_token *token)
{
	unsigned char *string = scanner->position;
	unsigned char first_char = *string;
	enum css_token_type type = CSS_TOKEN_GARBAGE;
	int real_length = -1;

	assert(first_char);
	token->string.string = string++;

	if (is_css_char_token(first_char)) {
		type = first_char;

	} else if (is_css_digit(first_char) || first_char == '.') {
		scan_css(scanner, string, CSS_CHAR_DIGIT);

		/* First scan the full number token */
		if (*string == '.') {
			string++;

			if (is_css_digit(*string)) {
				type = CSS_TOKEN_NUMBER;
				scan_css(scanner, string, CSS_CHAR_DIGIT);
			}
		}

		/* Check what kind of number we have */
		if (*string == '%') {
			if (first_char != '.')
				type = CSS_TOKEN_PERCENTAGE;
			string++;

		} else if (!is_css_ident_start(*string)) {
			type = CSS_TOKEN_NUMBER;

		} else {
			unsigned char *ident = string;

			scan_css(scanner, string, CSS_CHAR_IDENT);
			type = map_dom_scanner_string(scanner, ident, string,
						      CSS_TOKEN_DIMENSION);
		}

	} else if (is_css_ident_start(first_char)) {
		scan_css(scanner, string, CSS_CHAR_IDENT);

		if (*string == '(') {
			unsigned char *function_end = string + 1;

			/* Make sure that we have an ending ')' */
			skip_css(scanner, function_end, ')');
			if (*function_end == ')') {
				type = map_dom_scanner_string(scanner, token->string.string,
						string, CSS_TOKEN_FUNCTION);

				/* If it is not a known function just skip the
				 * how arg stuff so we don't end up generating
				 * a lot of useless tokens. */
				if (type == CSS_TOKEN_FUNCTION) {
					string = function_end;

				} else if (type == CSS_TOKEN_URL) {
					/* Extracting the URL first removes any
					 * leading or ending whitespace and
					 * then see if the url is given in a
					 * string. If that is the case the
					 * string delimiters are also trimmed.
					 * This is not totally correct because
					 * we should of course handle escape
					 * sequences .. but that will have to
					 * be fixed later.  */
					unsigned char *from = string + 1;
					unsigned char *to = function_end - 1;

					scan_css(scanner, from, CSS_CHAR_WHITESPACE);
					scan_back_css(scanner, to, CSS_CHAR_WHITESPACE);

					if (isquote(*from)) from++;
					if (isquote(*to)) to--;

					token->string.string = from;
					real_length = to - from + 1;
					assert(real_length >= 0);
					string = function_end;
				}

				assert(type != CSS_TOKEN_RGB || *string == '(');
				assert(type != CSS_TOKEN_URL || *string == ')');
				assert(type != CSS_TOKEN_FUNCTION || *string == ')');
			}

			string++;

		} else {
			type = CSS_TOKEN_IDENT;
		}

	} else if (!is_css_token_start(first_char)) {
		/* TODO: Better composing of error tokens. For now we just
		 * split them down into char tokens */

	} else if (first_char == '#') {
		/* Check whether it is hexcolor or hash token */
		if (is_css_hexdigit(*string)) {
			int hexdigits;

			scan_css(scanner, string, CSS_CHAR_HEX_DIGIT);

			/* Check that the hexdigit sequence is either 3 or 6
			 * chars and it isn't just start of some non-hex ident
			 * string. */
			hexdigits = string - token->string.string - 1;
			if ((hexdigits == 3 || hexdigits == 6)
			    && !is_css_ident(*string)) {
				type = CSS_TOKEN_HEX_COLOR;
			} else {
				scan_css(scanner, string, CSS_CHAR_IDENT);
				type = CSS_TOKEN_HASH;
			}

		} else if (is_css_ident(*string)) {
			/* Not *_ident_start() because hashes are #<name>. */
			scan_css(scanner, string, CSS_CHAR_IDENT);
			type = CSS_TOKEN_HASH;
		}

	} else if (first_char == '@') {
		/* Compose token containing @<ident> */
		if (is_css_ident_start(*string)) {
			unsigned char *ident = string;

			/* Scan both ident start and ident */
			scan_css(scanner, string, CSS_CHAR_IDENT);
			type = map_dom_scanner_string(scanner, ident, string,
						  CSS_TOKEN_AT_KEYWORD);
		}

	} else if (first_char == '*') {
		if (*string == '=') {
			type = CSS_TOKEN_SELECT_CONTAINS;
			string++;
		} else {
			type = CSS_TOKEN_IDENT;
		}

	} else if (first_char == '^') {
		if (*string == '=') {
			type = CSS_TOKEN_SELECT_BEGIN;
			string++;
		}

	} else if (first_char == '$') {
		if (*string == '=') {
			type = CSS_TOKEN_SELECT_END;
			string++;
		}

	} else if (first_char == '|') {
		if (*string == '=') {
			type = CSS_TOKEN_SELECT_HYPHEN_LIST;
			string++;
		}

	} else if (first_char == '!') {
		scan_css(scanner, string, CSS_CHAR_WHITESPACE);
		if (!c_strncasecmp(string, "important", 9)) {
			type = CSS_TOKEN_IMPORTANT;
			string += 9;
		}

	} else if (isquote(first_char)) {
		/* TODO: Escaped delimiters --jonas */
		int size = scanner->end - string;
		unsigned char *string_end = memchr(string, first_char, size);

		if (string_end) {
			/* We don't want the delimiters in the token */
			token->string.string++;
			real_length = string_end - token->string.string;
			string = string_end + 1;
			type = CSS_TOKEN_STRING;
		}

	} else if (first_char == '<' || first_char == '-') {
		/* Try to navigate SGML tagsoup */

		if (*string == '/') {
			/* Some kind of SGML tag end ... better bail out screaming */
			type = CSS_TOKEN_NONE;

		} else {
			unsigned char *sgml = string;

			/* Skip anything looking like SGML "<!--" and "-->"
			 * comments + <![CDATA[ and ]]> notations. */
			scan_css(scanner, sgml, CSS_CHAR_SGML_MARKUP);

			if (sgml - string >= 2
			    && ((first_char == '<' && *string == '!')
				|| (first_char == '-' && sgml[-1] == '>'))) {
				type = CSS_TOKEN_SKIP;
				string = sgml;
			}
		}

	} else if (first_char == '/') {
		/* Comments */
		if (*string == '*') {
			type = CSS_TOKEN_SKIP;

			for (string++; string < scanner->end; string++)
				if (*string == '*' && string[1] == '/') {
					string += 2;
					break;
				}
		}

	} else {
		INTERNAL("Someone forgot to put code for recognizing tokens "
			 "which start with '%c'.", first_char);
	}

	token->type = type;
	token->string.length = real_length > 0 ? real_length : string - token->string.string;
	token->precedence = get_css_precedence(type);
	scanner->position = string;
}

static struct dom_scanner_token *
scan_css_tokens(struct dom_scanner *scanner)
{
	struct dom_scanner_token *table_end = scanner->table + DOM_SCANNER_TOKENS;
	struct dom_scanner_token *current;

	if (!begin_dom_token_scanning(scanner))
		return get_dom_scanner_token(scanner);

	/* Scan tokens until we fill the table */
	for (current = scanner->table + scanner->tokens;
	     current < table_end && scanner->position < scanner->end;
	     current++) {
		scan_css(scanner, scanner->position, CSS_CHAR_WHITESPACE);
		if (scanner->position >= scanner->end) break;

		scan_css_token(scanner, current);

		/* Did some one scream for us to end the madness? */
		if (current->type == CSS_TOKEN_NONE) {
			scanner->position = NULL;
			current--;
			break;
		}

		/* Shall we scratch this token? */
		if (current->type == CSS_TOKEN_SKIP) {
 			current--;
		}
	}

	return end_dom_token_scanning(scanner, current);
}
