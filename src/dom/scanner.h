#ifndef EL_DOM_SCANNER_H
#define EL_DOM_SCANNER_H

#include "dom/string.h"
#include "util/error.h"

/* Define if you want a talking scanner */
/* #define DEBUG_DOM_SCANNER */

/* The {struct dom_scanner_token} describes one scanner state. There are two
 * kinds of tokens: char and non-char tokens. Char tokens contains only one
 * char and simply have their char value as type. They are tokens having
 * special control meaning in the code, like ':', ';', '{', '}' and '*'. Non
 * char tokens has one or more chars and contain stuff like number or
 * indentifier strings.  */
struct dom_scanner_token {
	/* The type the token */
	int type;

	/* Some precedence value */
	int precedence;

	/* The start of the token string and the token length */
	unsigned char *string;
	int length;
};

/* The naming of these two macros is a bit odd .. we compare often with
 * "static" strings (I don't have a better word) so the macro name should
 * be short. --jonas */

/* Compare the string of @token with @string */
#define dom_scanner_token_strlcasecmp(token, str, len) \
	((token) && !strlcasecmp((token)->string, (token)->length, str, len))

/* Also compares the token string but using a "static" string */
#define dom_scanner_token_contains(token, str) \
	dom_scanner_token_strlcasecmp(token, str, sizeof(str) - 1)


struct dom_scan_table_info {
	enum { DOM_SCAN_RANGE, DOM_SCAN_STRING, DOM_SCAN_END } type;
	struct dom_string data;
	int bits;
};

#define	DOM_SCAN_TABLE_SIZE	256

#define DOM_SCAN_TABLE_INFO(type, data1, data2, bits) \
	{ (type), INIT_DOM_STRING((data1), (data2)), (bits) }

#define DOM_SCAN_TABLE_RANGE(from, to, bits)	\
	DOM_SCAN_TABLE_INFO(DOM_SCAN_RANGE, from, to, bits)

#define DOM_SCAN_TABLE_STRING(str, bits)	\
	DOM_SCAN_TABLE_INFO(DOM_SCAN_STRING, str, sizeof(str) - 1, bits)

#define DOM_SCAN_TABLE_END			\
	DOM_SCAN_TABLE_INFO(DOM_SCAN_END, NULL, 0, 0)

struct dom_scanner_string_mapping {
	unsigned char *name;
	int type;
	int base_type;
};

struct dom_scanner;

struct dom_scanner_info {
	/* Table containing how to map strings to token types */
	const struct dom_scanner_string_mapping *mappings;

	/* Information for how to initialize the scanner table */
	const struct dom_scan_table_info *scan_table_info;

	/* Fills the scanner with tokens. Already scanned tokens which have not
	 * been requested remain and are moved to the start of the scanners
	 * token table. */
	/* Returns the current token or NULL if there are none. */
	struct dom_scanner_token *(*scan)(struct dom_scanner *scanner);

	/* The scanner table */
	/* Contains bitmaps for the various characters groups.
	 * Idea sync'ed from mozilla browser. */
	int scan_table[DOM_SCAN_TABLE_SIZE];

	/* Has the scanner info been initialized? */
	unsigned int initialized:1;
};


/* Initializes the scanner. */
void init_dom_scanner(struct dom_scanner *scanner, struct dom_scanner_info *scanner_info,
		      struct dom_string *string);

/* The number of tokens in the scanners token table:
 * At best it should be big enough to contain properties with space separated
 * values and function calls with up to 3 variables like rgb(). At worst it
 * should be no less than 2 in order to be able to peek at the next token in
 * the scanner. */
#define DOM_SCANNER_TOKENS 10

/* The {struct dom_scanner} describes the current state of the scanner. */
struct dom_scanner {
	/* The very start of the scanned string, the position in the string
	 * where to scan next and the end of the string. If position is NULL it
	 * means that no more tokens can be retrieved from the string. */
	unsigned char *string, *position, *end;

	/* The current token and number of scanned tokens in the table.
	 * If the number of scanned tokens is less than DOM_SCANNER_TOKENS it
	 * is because there are no more tokens in the string. */
	struct dom_scanner_token *current;
	int tokens;

	/* The 'meta' scanner information */
	struct dom_scanner_info *info;

#ifdef DEBUG_SCANNER
	/* Debug info about the caller. */
	unsigned char *file;
	int line;
#endif

	/* Some state indicator only meaningful to the scanner internals */
	int state;

	/* The table contain already scanned tokens. It is maintained in
	 * order to optimize the scanning a bit and make it possible to look
	 * ahead at the next token. You should always use the accessors
	 * (defined below) for getting tokens from the scanner. */
	struct dom_scanner_token table[DOM_SCANNER_TOKENS];
};

#define dom_scanner_has_tokens(scanner) \
	((scanner)->tokens > 0 && (scanner)->current < (scanner)->table + (scanner)->tokens)

/* This macro checks if the current scanner state is valid. Meaning if the
 * scanners table is full the last token skipping or get_next_scanner_token()
 * call made it possible to get the type of the next token. */
#define check_dom_scanner(scanner) \
	(scanner->tokens < DOM_SCANNER_TOKENS \
	 || scanner->current + 1 < scanner->table + scanner->tokens)


/* Scanner table accessors and mutators */

/* Checks the type of the next token */
#define check_next_dom_scanner_token(scanner, token_type)			\
	(scanner_has_tokens(scanner)						\
	 && ((scanner)->current + 1 < (scanner)->table + (scanner)->tokens)	\
	 && (scanner)->current[1].type == (token_type))

/* Access current and next token. Getting the next token might cause
 * a rescan so any token pointers that has been stored in a local variable
 * might not be valid after the call. */
static inline struct dom_scanner_token *
get_dom_scanner_token(struct dom_scanner *scanner)
{
	return dom_scanner_has_tokens(scanner) ? scanner->current : NULL;
}

/* Do a scanning if we do not have also have access to next token. */
static inline struct dom_scanner_token *
get_next_dom_scanner_token(struct dom_scanner *scanner)
{
	return (dom_scanner_has_tokens(scanner)
		&& (++scanner->current + 1 >= scanner->table + scanner->tokens)
		? scanner->info->scan(scanner) : get_dom_scanner_token(scanner));
}

/* This should just make the code more understandable .. hopefully */
#define skip_dom_scanner_token(scanner) get_next_dom_scanner_token(scanner)

/* Removes tokens from the scanner until it meets a token of the given type.
 * This token will then also be skipped. */
struct dom_scanner_token *
skip_dom_scanner_tokens(struct dom_scanner *scanner, int skipto, int precedence);

/* Looks up the string from @ident to @end to in the scanners string mapping
 * table */
int
map_dom_scanner_string(struct dom_scanner *scanner,
		       unsigned char *ident, unsigned char *end, int base_type);

#ifdef DEBUG_DOM_SCANNER
void dump_dom_scanner(struct dom_scanner *scanner);
#endif

/* The begin_token_scanning() and end_token_scanning() functions provide the
 * basic setup and teardown for the rescan function made public via the
 * scanner_info->scan member. */

/* Returns NULL if it is not necessary to try to scan for more tokens */
static inline struct dom_scanner_token *
begin_dom_token_scanning(struct dom_scanner *scanner)
{
	struct dom_scanner_token *table = scanner->table;
	struct dom_scanner_token *table_end = table + scanner->tokens;
	int move_to_front = int_max(table_end - scanner->current, 0);
	struct dom_scanner_token *current = move_to_front ? scanner->current : table;
	size_t moved_size = 0;

	assert(scanner->current);

	/* Move any untouched tokens */
	if (move_to_front) {
		moved_size = move_to_front * sizeof(*table);
		memmove(table, current, moved_size);
		current = &table[move_to_front];
	}

	/* Clear all unused tokens */
	memset(current, 0, sizeof(*table) * DOM_SCANNER_TOKENS - moved_size);

	if (!scanner->position) {
		scanner->tokens = move_to_front ? move_to_front : -1;
		scanner->current = table;
		assert(check_dom_scanner(scanner));
		return NULL;
	}

	scanner->tokens = move_to_front;

	return table;
}

/* Updates the @scanner struct after scanning has been done. The position
 * _after_ the last valid token is taken as the @end argument. */
/* It is ok for @end to be < scanner->table since scanner->tokens will become
 * <= 0 anyway. */
static inline struct dom_scanner_token *
end_dom_token_scanning(struct dom_scanner *scanner, struct dom_scanner_token *end)
{
	assert(end <= scanner->table + DOM_SCANNER_TOKENS);

	scanner->tokens = (end - scanner->table);
	scanner->current = scanner->table;
	if (scanner->position >= scanner->end)
		scanner->position = NULL;

	assert(check_dom_scanner(scanner));

	return get_dom_scanner_token(scanner);
}

#endif
