#ifndef EL__UTIL_SCANNER_H
#define EL__UTIL_SCANNER_H

#include "util/error.h"
#include "util/string.h"

/* Define if you want a talking scanner */
/* #define DEBUG_SCANNER */

/** The struct scanner_token describes one scanner state. There are two kinds
 * of tokens: char and non-char tokens. Char tokens contains only one char and
 * simply have their char value as type. They are tokens having special control
 * meaning in the code, like ':', ';', '{', '}' and '*'. Non char tokens has
 * one or more chars and contain stuff like number or indentifier strings.  */
struct scanner_token {
	/** The type of the token */
	int type;

	/** Some precedence value */
	int precedence;

	/** The start of the token string and the token length */
	unsigned char *string;
	int length;
};

/* The naming of these two macros is a bit odd .. we compare often with
 * "static" strings (I don't have a better word) so the macro name should
 * be short. --jonas */

/** Compare the string of @a token with @a str */
#define scanner_token_strlcasecmp(token, str, len) \
	((token) && !c_strlcasecmp((token)->string, (token)->length, str, len))

/** Also compares the token string but using a "static" string */
#define scanner_token_contains(token, str) \
	scanner_token_strlcasecmp(token, str, sizeof(str) - 1)


struct scan_table_info {
	enum { SCAN_RANGE, SCAN_STRING, SCAN_END } type;
	union scan_table_data {
		struct { unsigned char *source; long length; } string;
		struct { unsigned char *start; long end; } range;
	} data;
	int bits;
};

#define	SCAN_TABLE_SIZE	256

#define SCAN_TABLE_INFO(type, data1, data2, bits) \
	{ (type), { { (data1), (data2) } }, (bits) }

#define SCAN_TABLE_RANGE(from, to, bits) SCAN_TABLE_INFO(SCAN_RANGE, from, to, bits)
#define SCAN_TABLE_STRING(str, bits)	 SCAN_TABLE_INFO(SCAN_STRING, str, sizeof(str) - 1, bits)
#define SCAN_TABLE_END			 SCAN_TABLE_INFO(SCAN_END, 0, 0, 0)

struct scanner_string_mapping {
	unsigned char *name;
	int type;
	int base_type;
};

struct scanner;

struct scanner_info {
	/** Table containing how to map strings to token types */
	const struct scanner_string_mapping *mappings;

	/** Information for how to initialize the scanner table */
	const struct scan_table_info *scan_table_info;

	/** Fills the scanner with tokens. Already scanned tokens
	 * which have not been requested remain and are moved to the
	 * start of the scanners token table.
	 * @returns the current token or NULL if there are none. */
	struct scanner_token *(*scan)(struct scanner *scanner);

	/** The scanner table.  Contains bitmaps for the various
	 * characters groups.  Idea sync'ed from mozilla browser. */
	int scan_table[SCAN_TABLE_SIZE];

	/** Has the scanner info been initialized? */
	unsigned int initialized:1;
};


/** Initializes the scanner.
 * @relates scanner */
void init_scanner(struct scanner *scanner, struct scanner_info *scanner_info,
		  unsigned char *string, unsigned char *end);

/** The number of tokens in the scanners token table:
 * At best it should be big enough to contain properties with space separated
 * values and function calls with up to 3 variables like rgb(). At worst it
 * should be no less than 2 in order to be able to peek at the next token in
 * the scanner. */
#define SCANNER_TOKENS 10

/** The struct scanner describes the current state of the scanner. */
struct scanner {
	/** The very start of the scanned string, the position in the string
	 * where to scan next and the end of the string. If #position is NULL
	 * it means that no more tokens can be retrieved from the string. */
	unsigned char *string, *position, *end;

	/** The current token and number of scanned tokens in the table.
	 * If the number of scanned tokens is less than ::SCANNER_TOKENS
	 * it is because there are no more tokens in the string. */
	struct scanner_token *current;
	int tokens;

	/** The 'meta' scanner information */
	struct scanner_info *info;

#ifdef DEBUG_SCANNER
	/** @name Debug info about the caller.
	 * @{ */
	unsigned char *file;
	int line;
	/** @} */
#endif

	/** Some state indicator only meaningful to the scanner internals */
	int state;

	/** The table contain already scanned tokens. It is maintained in
	 * order to optimize the scanning a bit and make it possible to look
	 * ahead at the next token. You should always use the accessors
	 * (defined below) for getting tokens from the scanner. */
	struct scanner_token table[SCANNER_TOKENS];
};

/** @relates scanner  */
#define scanner_has_tokens(scanner) \
	((scanner)->tokens > 0 && (scanner)->current < (scanner)->table + (scanner)->tokens)

/** This macro checks if the current scanner state is valid. Meaning if the
 * scanners table is full the last token skipping or get_next_scanner_token()
 * call made it possible to get the type of the next token.
 * @relates scanner */
#define check_scanner(scanner) \
	(scanner->tokens < SCANNER_TOKENS \
	 || scanner->current + 1 < scanner->table + scanner->tokens)


/** @name Scanner table accessors and mutators
 * @{ */

/** Checks the type of the next token
 * @relates scanner */
#define check_next_scanner_token(scanner, token_type)				\
	(scanner_has_tokens(scanner)					\
	 && ((scanner)->current + 1 < (scanner)->table + (scanner)->tokens)	\
	 && (scanner)->current[1].type == (token_type))

/** Access current and next token. Getting the next token might cause
 * a rescan so any token pointers that has been stored in a local variable
 * might not be valid after the call.
 * @relates scanner */
static inline struct scanner_token *
get_scanner_token(struct scanner *scanner)
{
	return scanner_has_tokens(scanner) ? scanner->current : NULL;
}

/** Do a scanning if we do not have also have access to next token.
 * @relates scanner */
static inline struct scanner_token *
get_next_scanner_token(struct scanner *scanner)
{
	return (scanner_has_tokens(scanner)
		&& (++scanner->current + 1 >= scanner->table + scanner->tokens)
		? scanner->info->scan(scanner) : get_scanner_token(scanner));
}

/** This should just make the code more understandable .. hopefully
 * @relates scanner */
#define skip_scanner_token(scanner) get_next_scanner_token(scanner)

/** Removes tokens from the scanner until it meets a token of the given type.
 * This token will then also be skipped.
 * @relates scanner */
struct scanner_token *
skip_scanner_tokens(struct scanner *scanner, int skipto, int precedence);

/** @} */

/** Looks up the string from @a ident to @a end to in the scanners
 * string mapping table
 * @relates scanner */
int
map_scanner_string(struct scanner *scanner,
		   const unsigned char *ident, const unsigned char *end,
		   int base_type);

#ifdef DEBUG_SCANNER
/** @relates scanner */
void dump_scanner(struct scanner *scanner);
#endif

/* The begin_token_scanning() and end_token_scanning() functions provide the
 * basic setup and teardown for the rescan function made public via the
 * scanner_info->scan member.
 * @returns NULL if it is not necessary to try to scan for more tokens
 * @relates scanner */
static inline struct scanner_token *
begin_token_scanning(struct scanner *scanner)
{
	struct scanner_token *table = scanner->table;
	struct scanner_token *table_end = table + scanner->tokens;
	int move_to_front = int_max(table_end - scanner->current, 0);
	struct scanner_token *current = move_to_front ? scanner->current : table;
	size_t moved_size = 0;

	assert(scanner->current);

	/* Move any untouched tokens */
	if (move_to_front) {
		moved_size = move_to_front * sizeof(*table);
		memmove(table, current, moved_size);
		current = &table[move_to_front];
	}

	/* Clear all unused tokens */
	memset(current, 0, sizeof(*table) * SCANNER_TOKENS - moved_size);

	if (!scanner->position) {
		scanner->tokens = move_to_front ? move_to_front : -1;
		scanner->current = table;
		assert(check_scanner(scanner));
		return NULL;
	}

	scanner->tokens = move_to_front;

	return table;
}

/* Updates the @a scanner struct after scanning has been done. The position
 * _after_ the last valid token is taken as the @a end argument.
 *
 * It is ok for @a end to be < scanner->table since scanner->tokens
 * will become <= 0 anyway.
 * @relates scanner */
static inline struct scanner_token *
end_token_scanning(struct scanner *scanner, struct scanner_token *end)
{
	assert(end <= scanner->table + SCANNER_TOKENS);

	scanner->tokens = (end - scanner->table);
	scanner->current = scanner->table;
	if (scanner->position >= scanner->end)
		scanner->position = NULL;

	assert(check_scanner(scanner));

	return get_scanner_token(scanner);
}

#endif
