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
	DOM_SCAN_TABLE_STRING("\f\n\r",	 SGML_CHAR_NEWLINE),
	DOM_SCAN_TABLE_STRING("<&",	 SGML_CHAR_NOT_TEXT),
	DOM_SCAN_TABLE_STRING("<=>",	 SGML_CHAR_NOT_ATTRIBUTE),

	DOM_SCAN_TABLE_END,
};

#define SGML_STRING_MAP(str, type, family) \
	{ INIT_DOM_STRING(str, -1), SGML_TOKEN_##type, SGML_TOKEN_##family }

static struct dom_scanner_string_mapping sgml_string_mappings[] = {
	SGML_STRING_MAP("--",		NOTATION_COMMENT,	NOTATION),
	SGML_STRING_MAP("ATTLIST",	NOTATION_ATTLIST,	NOTATION),
	SGML_STRING_MAP("DOCTYPE",	NOTATION_DOCTYPE,	NOTATION),
	SGML_STRING_MAP("ELEMENT",	NOTATION_ELEMENT,	NOTATION),
	SGML_STRING_MAP("ENTITY",	NOTATION_ENTITY,	NOTATION),

	SGML_STRING_MAP("xml",		PROCESS_XML,		PROCESS),

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
#define	is_sgml_text(c)		!check_sgml_table(c, SGML_CHAR_NOT_TEXT)
#define	is_sgml_token_start(c)	check_sgml_table(c, SGML_CHAR_TOKEN_START)
#define	is_sgml_attribute(c)	!check_sgml_table(c, SGML_CHAR_NOT_ATTRIBUTE | SGML_CHAR_WHITESPACE)


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
		if (is_sgml_entity(*string)) {
			scan_sgml(scanner, string, SGML_CHAR_ENTITY);
			type = SGML_TOKEN_ENTITY;
			token->string.string++;
			real_length = string - token->string.string;
		}

		foreach_sgml_cdata (scanner, string) {
			if (*string == ';') {
				string++;
				break;
			}
		}

	} else {
		if (is_sgml_space(first_char)) {
			scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);
			type = string < scanner->end && is_sgml_text(*string)
			     ? SGML_TOKEN_TEXT : SGML_TOKEN_SPACE;
		} else {
			type = SGML_TOKEN_TEXT;
		}

		foreach_sgml_cdata (scanner, string) {
			/* m33p */;
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
			int length = scanner->end - pos;
			unsigned char *end = memchr(pos + 1, *pos, length);

			if (end) pos = end;
		}
	}

	*string = pos;
	return NULL;
}

static inline int
skip_comment(struct dom_scanner *scanner, unsigned char **string)
{
	unsigned char *pos = *string;
	int length = 0;

	for (; pos < scanner->end - 2; pos++)
		if (pos[0] == '-' && pos[1] == '-' && pos[2] == '>') {
			length = pos - *string;
			pos += 3;
			break;
		}

	*string = pos;
	return length;
}

static inline int
skip_cdata_section(struct dom_scanner *scanner, unsigned char **string)
{
	unsigned char *pos = *string;
	int length = 0;

	for (; pos < scanner->end - 3; pos++)
		if (pos[0] == ']' && pos[1] == ']' && pos[2] == '>') {
			length = pos - *string;
			pos += 3;
			break;
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

	token->string.string = string++;

	if (first_char == '<') {
		scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);

		if (scanner->state == SGML_STATE_ELEMENT) {
			/* Already inside an element so insert a tag end token
			 * and continue scanning in next iteration. */
			string--;
			real_length = 0;
			type = SGML_TOKEN_TAG_END;
			scanner->state = SGML_STATE_TEXT;

		} else if (is_sgml_ident(*string)) {
			token->string.string = string;
			scan_sgml(scanner, string, SGML_CHAR_IDENT);

			real_length = string - token->string.string;

			scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);
			if (*string == '>') {
				type = SGML_TOKEN_ELEMENT;
				string++;
			} else {
				scanner->state = SGML_STATE_ELEMENT;
				type = SGML_TOKEN_ELEMENT_BEGIN;
			}

		} else if (*string == '!') {
			unsigned char *ident;
			enum sgml_token_type base = SGML_TOKEN_NOTATION;

			string++;
			scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);
			token->string.string = ident = string;

			if (string + 1 < scanner->end
			    && string[0] == '-' && string[1] == '-') {
				string += 2;
				type = SGML_TOKEN_NOTATION_COMMENT;
				token->string.string = string;
				real_length = skip_comment(scanner, &string);
				assert(real_length >= 0);

			} else if (string + 6 < scanner->end
				   && !memcmp(string, "[CDATA[", 7)) {

				string += 7;
				type = SGML_TOKEN_CDATA_SECTION;
				token->string.string = string;
				real_length = skip_cdata_section(scanner, &string);
				assert(real_length >= 0);

			} else {
				scan_sgml(scanner, string, SGML_CHAR_IDENT);
				type = map_dom_scanner_string(scanner, ident, string, base);
				skip_sgml(scanner, &string, '>', 0);
			}

		} else if (*string == '?') {
			unsigned char *pos;
			enum sgml_token_type base = SGML_TOKEN_PROCESS;

			string++;
			scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);
			token->string.string = pos = string;
			scan_sgml(scanner, string, SGML_CHAR_IDENT);

			type = map_dom_scanner_string(scanner, pos, string, base);

			scanner->state = SGML_STATE_PROC_INST;

		} else if (*string == '/') {
			string++;
			scan_sgml(scanner, string, SGML_CHAR_WHITESPACE);

			if (is_sgml_ident(*string)) {
				token->string.string = string;
				scan_sgml(scanner, string, SGML_CHAR_IDENT);
				real_length = string - token->string.string;

				type = SGML_TOKEN_ELEMENT_END;
				skip_sgml(scanner, &string, '>', 1);

			} else if (*string == '>') {
				string++;
				real_length = 0;
				type = SGML_TOKEN_ELEMENT_END;
			}

			if (type != SGML_TOKEN_GARBAGE)
				scanner->state = SGML_STATE_TEXT;

		} else {
			/* Alien < > stuff so ignore it */
			skip_sgml(scanner, &string, '>', 0);
		}

	} else if (first_char == '=') {
		type = '=';

	} else if (first_char == '?' || first_char == '>') {
		if (first_char == '?') {
			skip_sgml(scanner, &string, '>', 0);
		}

		type = SGML_TOKEN_TAG_END;
		assert(scanner->state == SGML_STATE_ELEMENT);
		scanner->state = SGML_STATE_TEXT;

	} else if (first_char == '/') {
		if (*string == '>') {
			string++;
			real_length = 0;
			type = SGML_TOKEN_ELEMENT_EMPTY_END;
			assert(scanner->state == SGML_STATE_ELEMENT);
			scanner->state = SGML_STATE_TEXT;
		} else if (is_sgml_attribute(*string)) {
			scan_sgml_attribute(scanner, string);
			type = SGML_TOKEN_ATTRIBUTE;
			if (string[-1] == '/' && string[0] == '>')
				string--;
		}

	} else if (isquote(first_char)) {
		int size = scanner->end - string;
		unsigned char *string_end = memchr(string, first_char, size);

		if (string_end) {
			/* We don't want the delimiters in the token */
			token->string.string++;
			real_length = string_end - token->string.string;
			string = string_end + 1;
			type = SGML_TOKEN_STRING;
		} else if (is_sgml_attribute(*string)) {
			token->string.string++;
			scan_sgml_attribute(scanner, string);
			type = SGML_TOKEN_ATTRIBUTE;
		}

	} else if (is_sgml_attribute(first_char)) {
		if (is_sgml_ident(first_char)) {
			scan_sgml(scanner, string, SGML_CHAR_IDENT);
			type = SGML_TOKEN_IDENT;
		}

		if (is_sgml_attribute(*string)) {
			scan_sgml_attribute(scanner, string);
			type = SGML_TOKEN_ATTRIBUTE;
			if (string[-1] == '/' && string[0] == '>')
				string--;
		}
	}

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
	size_t size;

	token->string.string = string;

	/* Figure out where the processing instruction ends. This doesn't use
	 * skip_sgml() since we MUST ignore precedence here to allow '<' inside
	 * the data part to be skipped correctly. */
	for (size = scanner->end - string;
	     size > 0 && (string = memchr(string, '>', size));
	     string++) {
		if (string[-1] == '?') {
			string++;
			break;
		}
	}

	if (!string) string = scanner->end;

	token->type = SGML_TOKEN_PROCESS_DATA;
	token->string.length = string - token->string.string - 2;
	token->precedence = get_sgml_precedence(token->type);
	scanner->position = string;
	scanner->state = SGML_STATE_TEXT;
}


/* Scanner multiplexor */

static struct dom_scanner_token *
scan_sgml_tokens(struct dom_scanner *scanner)
{
	struct dom_scanner_token *table_end = scanner->table + DOM_SCANNER_TOKENS;
	struct dom_scanner_token *current;

	if (!begin_dom_token_scanning(scanner))
		return get_dom_scanner_token(scanner);

	/* Scan tokens until we fill the table */
	for (current = scanner->table + scanner->tokens;
	     current < table_end && scanner->position < scanner->end;
	     current++) {
		if (scanner->state == SGML_STATE_ELEMENT
		    || (*scanner->position == '<'
			&& scanner->state != SGML_STATE_PROC_INST)) {
			scan_sgml(scanner, scanner->position, SGML_CHAR_WHITESPACE);
			if (scanner->position >= scanner->end) break;

			scan_sgml_element_token(scanner, current);

			/* Shall we scratch this token? */
			if (current->type == SGML_TOKEN_SKIP) {
				current--;
			}

		} else if (scanner->state == SGML_STATE_TEXT) {
			scan_sgml_text_token(scanner, current);

		} else {
			scan_sgml(scanner, scanner->position, SGML_CHAR_WHITESPACE);
			scan_sgml_proc_inst_token(scanner, current);
		}
	}

	return end_dom_token_scanning(scanner, current);
}
