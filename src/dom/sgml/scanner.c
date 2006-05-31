/* SGML token scanner utilities */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "dom/scanner.h"
#include "dom/sgml/scanner.h"
#include "dom/string.h"
#include "util/error.h"


/* Bitmap entries for the SGML character groups used in the scanner table */

enum sgml_char_group {
	SGML_CHAR_ENTITY	= (1 << 1),
	SGML_CHAR_IDENT		= (1 << 2),
	SGML_CHAR_NEWLINE	= (1 << 3),
	SGML_CHAR_WHITESPACE	= (1 << 4),
	SGML_CHAR_NOT_TEXT	= (1 << 5),
	SGML_CHAR_NOT_ATTRIBUTE	= (1 << 6),
};

static struct dom_scan_table_info sgml_scan_table_info[] = {
	DOM_SCAN_TABLE_RANGE("0", '9', SGML_CHAR_IDENT | SGML_CHAR_ENTITY),
	DOM_SCAN_TABLE_RANGE("A", 'Z', SGML_CHAR_IDENT | SGML_CHAR_ENTITY),
	DOM_SCAN_TABLE_RANGE("a", 'z', SGML_CHAR_IDENT | SGML_CHAR_ENTITY),
	/* For the octal number impared (me including) \241 is 161 --jonas */
	DOM_SCAN_TABLE_RANGE("\241", 255, SGML_CHAR_IDENT | SGML_CHAR_ENTITY),

	DOM_SCAN_TABLE_STRING("-_:.",	 SGML_CHAR_IDENT | SGML_CHAR_ENTITY),
	DOM_SCAN_TABLE_STRING("#",	 SGML_CHAR_ENTITY),
	DOM_SCAN_TABLE_STRING(" \f\n\r\t\v", SGML_CHAR_WHITESPACE),
	DOM_SCAN_TABLE_STRING("\f\n",	 SGML_CHAR_NEWLINE),
	DOM_SCAN_TABLE_STRING("<&",	 SGML_CHAR_NOT_TEXT),
	DOM_SCAN_TABLE_STRING("<=>",	 SGML_CHAR_NOT_ATTRIBUTE),

	DOM_SCAN_TABLE_END,
};

#define SGML_STRING_MAP(str, type, family) \
	{ STATIC_DOM_STRING(str), SGML_TOKEN_##type, SGML_TOKEN_##family }

static struct dom_scanner_string_mapping sgml_string_mappings[] = {
	SGML_STRING_MAP("--",		  NOTATION_COMMENT,	  NOTATION),
	SGML_STRING_MAP("ATTLIST",	  NOTATION_ATTLIST,	  NOTATION),
	SGML_STRING_MAP("DOCTYPE",	  NOTATION_DOCTYPE,	  NOTATION),
	SGML_STRING_MAP("ELEMENT",	  NOTATION_ELEMENT,	  NOTATION),
	SGML_STRING_MAP("ENTITY",	  NOTATION_ENTITY,	  NOTATION),

	SGML_STRING_MAP("xml",		  PROCESS_XML,		  PROCESS),
	SGML_STRING_MAP("xml-stylesheet", PROCESS_XML_STYLESHEET, PROCESS),

	DOM_STRING_MAP_END,
};

static struct dom_scanner_token *scan_sgml_tokens(struct dom_scanner *scanner);

struct dom_scanner_info sgml_scanner_info = {
	sgml_string_mappings,
	sgml_scan_table_info,
	scan_sgml_tokens,
};

#define	check_sgml_table(c, bit)	(sgml_scanner_info.scan_table[(c)] & (bit))

#define	scan_sgml(scanner, s, bit)					\
	while ((s) < (scanner)->end && check_sgml_table(*(s), bit)) (s)++;

#define	is_sgml_ident(c)	check_sgml_table(c, SGML_CHAR_IDENT)
#define	is_sgml_entity(c)	check_sgml_table(c, SGML_CHAR_ENTITY)
#define	is_sgml_space(c)	check_sgml_table(c, SGML_CHAR_WHITESPACE)
#define	is_sgml_newline(c)	check_sgml_table(c, SGML_CHAR_NEWLINE)
#define	is_sgml_text(c)		!check_sgml_table(c, SGML_CHAR_NOT_TEXT)
#define	is_sgml_token_start(c)	check_sgml_table(c, SGML_CHAR_TOKEN_START)
#define	is_sgml_attribute(c)	!check_sgml_table(c, SGML_CHAR_NOT_ATTRIBUTE | SGML_CHAR_WHITESPACE)

static inline void
skip_sgml_space(struct dom_scanner *scanner, unsigned char **string)
{
	unsigned char *pos = *string;

	if (!scanner->count_lines) {
		scan_sgml(scanner, pos, SGML_CHAR_WHITESPACE);
	} else {
		while (pos < scanner->end && is_sgml_space(*pos)) {
			if (is_sgml_newline(*pos))
				scanner->lineno++;
			pos++;
		}
	}

	*string = pos;
}

#define check_sgml_incomplete(scanner, string) \
	((scanner)->check_complete \
	 && (scanner)->incomplete \
	 && (string) == (scanner)->end)

static void
set_sgml_incomplete(struct dom_scanner *scanner, struct dom_scanner_token *token)
{
	size_t left = scanner->end - scanner->position;

	assert(left > 0);

	token->type = SGML_TOKEN_INCOMPLETE;
	set_dom_string(&token->string, scanner->position, left);

	/* Stop the scanning. */
	scanner->position = scanner->end;
}


static inline int
check_sgml_error(struct dom_scanner *scanner)
{
	unsigned int found_error = scanner->found_error;

	/* Toggle if we found an error previously. */
	scanner->found_error = 0;

	return scanner->detect_errors && !found_error;
}

static unsigned char *
get_sgml_error_end(struct dom_scanner *scanner, enum sgml_token_type type,
		   unsigned char *end)
{
	switch (type) {
	case SGML_TOKEN_CDATA_SECTION:
	case SGML_TOKEN_NOTATION_ATTLIST:
	case SGML_TOKEN_NOTATION_DOCTYPE:
	case SGML_TOKEN_NOTATION_ELEMENT:
		if (scanner->position + 9 < end)
			end = scanner->position + 9;
		break;

	case SGML_TOKEN_NOTATION_COMMENT:
		/* Just include the '<!--' part. */
		if (scanner->position + 4 < end)
			end = scanner->position + 4;
		break;

	case SGML_TOKEN_NOTATION_ENTITY:
		if (scanner->position + 6 < end)
			end = scanner->position + 6;
		break;

	case SGML_TOKEN_PROCESS_XML:
		if (scanner->position + 5 < end)
			end = scanner->position + 5;
		break;

	case SGML_TOKEN_PROCESS_XML_STYLESHEET:
		if (scanner->position + 16 < end)
			end = scanner->position + 16;
		break;

	default:
		break;
	}

	return end;
}


static struct dom_scanner_token *
set_sgml_error(struct dom_scanner *scanner, unsigned char *end)
{
	struct dom_scanner_token *token = scanner->current;
	struct dom_scanner_token *next;

	assert(!scanner->found_error);

	if (scanner->current >= scanner->table + DOM_SCANNER_TOKENS) {
		scanner->found_error = 1;
		next = NULL;

	} else {
		scanner->current++;
		next = scanner->current;
		copy_struct(next, token);
	}

	token->type = SGML_TOKEN_ERROR;
	token->lineno = scanner->lineno;
	set_dom_string(&token->string, scanner->position, end - scanner->position);

	return next;
}


/* Text token scanning */

/* I think it is faster to not check the table here --jonas */
#define foreach_sgml_cdata(scanner, str)				\
	for (; ((str) < (scanner)->end && *(str) != '<' && *(str) != '&'); (str)++)

static inline void
scan_sgml_text_token(struct dom_scanner *scanner, struct dom_scanner_token *token)
{
	unsigned char *string = scanner->position;
	unsigned char first_char = *string;
	enum sgml_token_type type = SGML_TOKEN_GARBAGE;
	int real_length = -1;

	/* In scan_sgml_tokens() we check that first_char != '<' */
	assert(first_char != '<' && scanner->state == SGML_STATE_TEXT);

	token->string.string = string++;

	if (first_char == '&') {
		int complete = 0;

		if (is_sgml_entity(*string)) {
			scan_sgml(scanner, string, SGML_CHAR_ENTITY);
			type = SGML_TOKEN_ENTITY;
			token->string.string++;
			real_length = string - token->string.string;
		}

		foreach_sgml_cdata (scanner, string) {
			if (*string == ';') {
				complete = 1;
				string++;
				break;
			}
		}

		/* We want the biggest possible text token. */
		if (!complete) {
			if (check_sgml_incomplete(scanner, string)) {
				set_sgml_incomplete(scanner, token);
				return;
			}

			if (check_sgml_error(scanner)) {
				token = set_sgml_error(scanner, string);
				if (!token)
					return;
			}
		}

	} else {
		if (is_sgml_space(first_char)) {
			if (scanner->count_lines
			    && is_sgml_newline(first_char))
				scanner->lineno++;

			skip_sgml_space(scanner, &string);
			type = string < scanner->end && is_sgml_text(*string)
			     ? SGML_TOKEN_TEXT : SGML_TOKEN_SPACE;
		} else {
			type = SGML_TOKEN_TEXT;
		}

		if (scanner->count_lines) {
			foreach_sgml_cdata (scanner, string) {
				if (is_sgml_newline(*string))
					scanner->lineno++;
			}
		} else {
			foreach_sgml_cdata (scanner, string) {
				/* m33p */;
			}
		}

		/* We want the biggest possible text token. */
		if (check_sgml_incomplete(scanner, string)) {
			set_sgml_incomplete(scanner, token);
			return;
		}
	}

	token->type = type;
	token->string.length = real_length >= 0 ? real_length : string - token->string.string;
	token->precedence = get_sgml_precedence(type);
	scanner->position = string;
}


/* Element scanning */

/* Check whether it is safe to skip the @token when looking for @skipto. */
static inline int
check_sgml_precedence(int type, int skipto)
{
	return get_sgml_precedence(type) <= get_sgml_precedence(skipto);
}

/* Skip until @skipto is found, without taking precedence into account. */
static inline unsigned char *
skip_sgml_chars(struct dom_scanner *scanner, unsigned char *string,
		unsigned char skipto)
{
	int newlines;

	assert(string >= scanner->position && string <= scanner->end);

	if (!scanner->count_lines) {
		size_t length = scanner->end - string;

		return memchr(string, skipto, length);
	}

	for (newlines = 0; string < scanner->end; string++) {
		if (is_sgml_newline(*string))
			newlines++;
		if (*string == skipto) {
			/* Only count newlines if we actually find the
			 * requested char. Else callers are assumed to discard
			 * the scanning. */
			scanner->lineno += newlines;
			return string;
		}
	}

	return NULL;
}

/* XXX: Only element or ``in tag'' precedence is handled correctly however
 * using this function for CDATA or text would be overkill. */
static inline unsigned char *
skip_sgml(struct dom_scanner *scanner, unsigned char **string, unsigned char skipto,
	  int check_quoting)
{
	unsigned char *pos = *string;

	for (; pos < scanner->end; pos++) {
		if (*pos == skipto) {
			*string = pos + 1;
			return pos;
		}

		if (!check_sgml_precedence(*pos, skipto))
			break;

		if (check_quoting && isquote(*pos)) {
			unsigned char *end;

			end = skip_sgml_chars(scanner, pos + 1, *pos);
			if (end) pos = end;

		} else if (scanner->count_lines && is_sgml_newline(*pos)) {
			scanner->lineno++;
		}
	}

	*string = pos;
	return NULL;
}

static inline int
skip_sgml_comment(struct dom_scanner *scanner, unsigned char **string,
		  int *possibly_incomplete)
{
	unsigned char *pos = *string;
	int length = 0;

	for ( ; (pos = skip_sgml_chars(scanner, pos, '>')); pos++) {
		/* It is always safe to access index -2 and -1 here since we
		 * are supposed to have '<!--' before this is called. We do
		 * however need to check that the '-->' are not overlapping any
		 * preceeding '-'. Additionally also handle the quirky '--!>'
		 * end sometimes found. */
		if (pos[-2] == '-') {
			if (pos[-1] == '-' && &pos[-2] >= *string) {
				length = pos - *string - 2;
				*possibly_incomplete = 0;
				pos++;
				break;
			} else if (pos[-1] == '!' && pos[-3] == '-' && &pos[-3] >= *string) {
				length = pos - *string - 3;
				*possibly_incomplete = 0;
				pos++;
				break;
			}
		}
	}

	if (!pos) {
		pos = scanner->end;
		/* The token is incomplete but set the length to handle tag
		 * tag soup graciously. */
		*possibly_incomplete = 1;
		length = pos - *string;
	}

	*string = pos;
	return length;
}

static inline int
skip_sgml_cdata_section(struct dom_scanner *scanner, unsigned char **string,
		  	int *possibly_incomplete)
{
	unsigned char *pos = *string;
	int length = 0;

	for ( ; (pos = skip_sgml_chars(scanner, pos, '>')); pos++) {
		/* It is always safe to access index -2 and -1 here since we
		 * are supposed to have '<![CDATA[' before this is called. */
		if (pos[-2] == ']' && pos[-1] == ']') {
			length = pos - *string - 2;
			*possibly_incomplete = 0;
			pos++;
			break;
		}
	}

	if (!pos) {
		pos = scanner->end;
		/* The token is incomplete but set the length to handle tag
		 * soup graciously. */
		*possibly_incomplete = 1;
		length = pos - *string;
	}

	*string = pos;
	return length;
}

#define scan_sgml_attribute(scanner, str)				\
	while ((str) < (scanner)->end && is_sgml_attribute(*(str)))	\
	       (str)++;

static inline void
scan_sgml_element_token(struct dom_scanner *scanner, struct dom_scanner_token *token)
{
	unsigned char *string = scanner->position;
	unsigned char first_char = *string;
	enum sgml_token_type type = SGML_TOKEN_GARBAGE;
	int real_length = -1;
	int possibly_incomplete = 1;
	enum sgml_scanner_state scanner_state = scanner->state;

	token->string.string = string++;

	if (first_char == '<') {
		skip_sgml_space(scanner, &string);

		if (scanner->state == SGML_STATE_ELEMENT) {
			/* Already inside an element so insert a tag end token
			 * and continue scanning in next iteration. */
			type = SGML_TOKEN_TAG_END;
			scanner_state = SGML_STATE_TEXT;

			/* We are creating a 'virtual' that has no source. */
			possibly_incomplete = 0;
			string = token->string.string;
			real_length = 0;

		} else if (string == scanner->end) {
			/* It is incomplete so prevent out of bound acess to
			 * the scanned string. */

		} else if (is_sgml_ident(*string)) {
			token->string.string = string;
			scan_sgml(scanner, string, SGML_CHAR_IDENT);

			real_length = string - token->string.string;

			skip_sgml_space(scanner, &string);
			if (string < scanner->end && *string == '>') {
				type = SGML_TOKEN_ELEMENT;
				string++;

				/* We found the end. */
				possibly_incomplete = 0;

			} else {
				/* Was any space skipped? */
				if (is_sgml_space(string[-1])) {
					/* We found the end. */
					possibly_incomplete = 0;
				}
				type = SGML_TOKEN_ELEMENT_BEGIN;
				scanner_state = SGML_STATE_ELEMENT;
			}

		} else if (*string == '!') {
			unsigned char *ident;
			enum sgml_token_type base = SGML_TOKEN_NOTATION;

			string++;
			skip_sgml_space(scanner, &string);
			token->string.string = ident = string;

			if (string + 1 < scanner->end
			    && string[0] == '-' && string[1] == '-') {
				string += 2;
				type = SGML_TOKEN_NOTATION_COMMENT;
				token->string.string = string;
				real_length = skip_sgml_comment(scanner, &string,
								&possibly_incomplete);
				assert(real_length >= 0);

			} else if (string + 6 < scanner->end
				   && !memcmp(string, "[CDATA[", 7)) {

				string += 7;
				type = SGML_TOKEN_CDATA_SECTION;
				token->string.string = string;
				real_length = skip_sgml_cdata_section(scanner, &string,
								      &possibly_incomplete);
				assert(real_length >= 0);

			} else {
				scan_sgml(scanner, string, SGML_CHAR_IDENT);
				type = map_dom_scanner_string(scanner, ident, string, base);
				if (skip_sgml(scanner, &string, '>', 0)) {
					/* We found the end. */
					possibly_incomplete = 0;
				}
			}

		} else if (*string == '?') {
			unsigned char *pos;
			enum sgml_token_type base = SGML_TOKEN_PROCESS;

			string++;
			skip_sgml_space(scanner, &string);
			token->string.string = pos = string;
			scan_sgml(scanner, string, SGML_CHAR_IDENT);

			type = map_dom_scanner_string(scanner, pos, string, base);

			scanner_state = SGML_STATE_PROC_INST;

			real_length = string - token->string.string;
			skip_sgml_space(scanner, &string);

			/* Make '<?xml ' cause the right kind of error. */
			if (is_sgml_space(string[-1])
			    && string < scanner->end) {
				/* We found the end. */
				possibly_incomplete = 0;
			}

			if (scanner->check_complete && scanner->incomplete) {
				/* We need to fit both the process target token
				 * and the process data token into the scanner
				 * table. */
				if (token + 1 >= scanner->table + DOM_SCANNER_TOKENS) {
					possibly_incomplete = 1;

				} else if (!possibly_incomplete) {
					/* FIXME: We do this twice. */
					for (pos = string + 1;
					     (pos = skip_sgml_chars(scanner, pos, '>'));
					     pos++) {
						if (pos[-1] == '?')
							break;
					}
					if (!pos)
						possibly_incomplete = 1;
				}

				if (possibly_incomplete)
					string = scanner->end;
			}

		} else if (*string == '/') {
			string++;
			skip_sgml_space(scanner, &string);

			if (string == scanner->end) {
				/* Prevent out of bound access. */

			} else if (is_sgml_ident(*string)) {
				token->string.string = string;
				scan_sgml(scanner, string, SGML_CHAR_IDENT);
				real_length = string - token->string.string;

				type = SGML_TOKEN_ELEMENT_END;
				if (skip_sgml(scanner, &string, '>', 1)) {
					/* We found the end. */
					possibly_incomplete = 0;
				}

			} else if (*string == '>') {
				string++;
				real_length = 0;
				type = SGML_TOKEN_ELEMENT_END;

				/* We found the end. */
				possibly_incomplete = 0;
			}

			if (type != SGML_TOKEN_GARBAGE) {
				scanner_state = SGML_STATE_TEXT;
			}

		} else {
			/* Alien < > stuff so ignore it */
			if (skip_sgml(scanner, &string, '>', 0)) {
				/* We found the end. */
				possibly_incomplete = 0;
			}
		}

	} else if (first_char == '=') {
		type = '=';
		/* We found the end. */
		possibly_incomplete = 0;

	} else if (first_char == '?' || first_char == '>') {
		if (first_char == '?') {
			if (skip_sgml(scanner, &string, '>', 0)) {
				/* We found the end. */
				possibly_incomplete = 0;
			}
		} else {
			assert(first_char == '>');

			/* We found the end. */
			possibly_incomplete = 0;
		}

		type = SGML_TOKEN_TAG_END;
		assert(scanner->state == SGML_STATE_ELEMENT);
		scanner_state = SGML_STATE_TEXT;

	} else if (first_char == '/') {
		/* We allow '/' inside elements and only consider it as an end
		 * tag if immediately preceeds the '>' char. This is to allow
		 *
		 *	'<form action=/ >'	where '/' is part of a path and
		 *	'<form action=a />'	where '/>' is truely a tag end
		 *
		 * For stricter parsing we should always require attribute
		 * values to be quoted.
		 */
		if (string == scanner->end) {
			/* Prevent out of bound access. */

 		} else if (*string == '>') {
			string++;
			real_length = 0;
			type = SGML_TOKEN_ELEMENT_EMPTY_END;
			assert(scanner->state == SGML_STATE_ELEMENT);
			scanner_state = SGML_STATE_TEXT;

			/* We found the end. */
			possibly_incomplete = 0;

		} else if (is_sgml_attribute(*string)) {
			scan_sgml_attribute(scanner, string);
			type = SGML_TOKEN_ATTRIBUTE;
			if (string[-1] == '/' && string[0] == '>') {
				string--;
				/* We found the end. */
				possibly_incomplete = 0;
			}
		}

	} else if (isquote(first_char)) {
		unsigned char *string_end = skip_sgml_chars(scanner, string, first_char);

		if (string_end) {
			/* We don't want the delimiters in the token */
			token->string.string++;
			real_length = string_end - token->string.string;
			string = string_end + 1;
			type = SGML_TOKEN_STRING;

			/* We found the end. */
			possibly_incomplete = 0;

		} else if (scanner->check_complete && scanner->incomplete) {
			/* Force an incomplete token. */
			string = scanner->end;

		} else if (string < scanner->end
			   && is_sgml_attribute(*string)) {
			token->string.string++;
			scan_sgml_attribute(scanner, string);
			type = SGML_TOKEN_ATTRIBUTE;
		}

	} else if (is_sgml_attribute(first_char)) {
		if (is_sgml_ident(first_char)) {
			scan_sgml(scanner, string, SGML_CHAR_IDENT);
			type = SGML_TOKEN_IDENT;
		}

		if (string < scanner->end
		    && is_sgml_attribute(*string)) {
			scan_sgml_attribute(scanner, string);
			type = SGML_TOKEN_ATTRIBUTE;
			if (string[-1] == '/' && string[0] == '>') {
				/* We found the end. */
				possibly_incomplete = 0;
				string--;
			}
		}
	}

	if (possibly_incomplete) {
		if (check_sgml_incomplete(scanner, string)) {
			set_sgml_incomplete(scanner, token);
			return;
		}

		if (check_sgml_error(scanner) && string == scanner->end) {
			unsigned char *end;

			end = get_sgml_error_end(scanner, type, string);
			token = set_sgml_error(scanner, end);
			if (!token)
				return;
		}
	}

	/* Only apply the state change if the token was not abandoned because
	 * it was incomplete. */
	scanner->state = scanner_state;

	token->type = type;
	token->string.length = real_length >= 0 ? real_length : string - token->string.string;
	token->precedence = get_sgml_precedence(type);
	scanner->position = string;
}


/* Processing instruction data scanning */

static inline void
scan_sgml_proc_inst_token(struct dom_scanner *scanner, struct dom_scanner_token *token)
{
	unsigned char *string = scanner->position;
	/* The length can be empty for '<??>'. */
	ssize_t length = -1;

	token->string.string = string++;

	/* Figure out where the processing instruction ends. This doesn't use
	 * skip_sgml() since we MUST ignore precedence here to allow '<' inside
	 * the data part to be skipped correctly. */
	for ( ; (string = skip_sgml_chars(scanner, string, '>')); string++) {
		if (string[-1] == '?') {
			string++;
			length = string - token->string.string - 2;
			break;
		}
	}

	if (!string) {
		/* Makes the next succeed when checking for incompletion, and
		 * puts the rest of the text within the token. */
		string = scanner->end;

		if (check_sgml_incomplete(scanner, string)) {
			set_sgml_incomplete(scanner, token);
			return;
		}

		if (check_sgml_error(scanner)) {
			token = set_sgml_error(scanner, string);
			if (!token)
				return;
		}
	}

	token->type = SGML_TOKEN_PROCESS_DATA;
	token->string.length = length >= 0 ? length : string - token->string.string;
	token->precedence = get_sgml_precedence(token->type);
	scanner->position = string;
	scanner->state = SGML_STATE_TEXT;
}


/* Scanner multiplexor */

static struct dom_scanner_token *
scan_sgml_tokens(struct dom_scanner *scanner)
{
	struct dom_scanner_token *table_end = scanner->table + DOM_SCANNER_TOKENS;

	if (!begin_dom_token_scanning(scanner))
		return get_dom_scanner_token(scanner);

	/* Scan tokens until we fill the table */
	for (scanner->current = scanner->table + scanner->tokens;
	     scanner->current < table_end && scanner->position < scanner->end;
	     scanner->current++) {
		if (scanner->state == SGML_STATE_ELEMENT
		    || (*scanner->position == '<'
			&& scanner->state != SGML_STATE_PROC_INST)) {
			skip_sgml_space(scanner, &scanner->position);
			if (scanner->position >= scanner->end) break;

			scan_sgml_element_token(scanner, scanner->current);

			/* Shall we scratch this token? */
			if (scanner->current->type == SGML_TOKEN_SKIP) {
				scanner->current--;
			}

		} else if (scanner->state == SGML_STATE_TEXT) {
			scan_sgml_text_token(scanner, scanner->current);

		} else {
			scan_sgml_proc_inst_token(scanner, scanner->current);
		}
	}

	return end_dom_token_scanning(scanner, scanner->current);
}
