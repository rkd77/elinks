/** A pretty generic scanner
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "util/error.h"
#include "util/scanner.h"
#include "util/string.h"


int
map_scanner_string(struct scanner *scanner,
		   const unsigned char *ident, const unsigned char *end,
		   int base_type)
{
	const struct scanner_string_mapping *mappings = scanner->info->mappings;
	int length = end - ident;

	for (; mappings->name; mappings++) {
		if (mappings->base_type == base_type
		    && !c_strlcasecmp(mappings->name, -1, ident, length))
			return mappings->type;
	}

	return base_type;
}


struct scanner_token *
skip_scanner_tokens(struct scanner *scanner, int skipto, int precedence)
{
	struct scanner_token *token = get_scanner_token(scanner);

	/* Skip tokens while handling some basic precedens of special chars
	 * so we don't skip to long. */
	while (token) {
		if (token->type == skipto
		    || token->precedence > precedence)
			break;
		token = get_next_scanner_token(scanner);
	}

	return (token && token->type == skipto)
		? get_next_scanner_token(scanner) : NULL;
}

#ifdef DEBUG_SCANNER
void
dump_scanner(struct scanner *scanner)
{
	unsigned char buffer[MAX_STR_LEN];
	struct scanner_token *token = scanner->current;
	struct scanner_token *table_end = scanner->table + scanner->tokens;
	unsigned char *srcpos = token->string, *bufpos = buffer;
	int src_lookahead = 50;
	int token_lookahead = 4;
	int srclen;

	if (!scanner_has_tokens(scanner)) return;

	memset(buffer, 0, MAX_STR_LEN);
	for (; token_lookahead > 0 && token < table_end; token++, token_lookahead--) {
		int buflen = MAX_STR_LEN - (bufpos - buffer);
		int added = snprintf(bufpos, buflen, "[%.*s] ", token->length, token->string);

		bufpos += added;
	}

	if (scanner->tokens > token_lookahead) {
		memcpy(bufpos, "... ", 4);
		bufpos += 4;
	}

	srclen = strlen(srcpos);
	int_upper_bound(&src_lookahead, srclen);
	*bufpos++ = '[';

	/* Compress the lookahead string */
	for (; src_lookahead > 0; src_lookahead--, srcpos++, bufpos++) {
		if (*srcpos == '\n' || *srcpos == '\r' || *srcpos == '\t') {
			*bufpos++ = '\\';
			*bufpos = *srcpos == '\n' ? 'n'
				: (*srcpos == '\r' ? 'r' : 't');
		} else {
			*bufpos = *srcpos;
		}
	}

	if (srclen > src_lookahead)
		memcpy(bufpos, "...]", 4);
	else
		memcpy(bufpos, "]", 2);

	errfile = scanner->file, errline = scanner->line;
	elinks_wdebug("%s", buffer);
}

struct scanner_token *
get_scanner_token_debug(struct scanner *scanner)
{
	if (!scanner_has_tokens(scanner)) return NULL;

	dump_scanner(scanner);

	/* Make sure we do not return invalid tokens */
	assert(!scanner_has_tokens(scanner)
		|| scanner->current->type != 0);

	return get_scanner_token(scanner);
}
#endif


/** @name Initializers
 * @{ */

static inline void
init_scanner_info(struct scanner_info *scanner_info)
{
	const struct scan_table_info *info = scanner_info->scan_table_info;
	int *scan_table = scanner_info->scan_table;
	int i;

	if (!info) return;

	for (i = 0; info[i].type != SCAN_END; i++) {
		const union scan_table_data *data = &info[i].data;

		if (info[i].type == SCAN_RANGE) {
			int index = *data->range.start;

			assert(index > 0);
			assert(data->range.end < SCAN_TABLE_SIZE);
			assert(index <= data->range.end);

			for (; index <= data->range.end; index++)
				scan_table[index] |= info[i].bits;

		} else {
			unsigned char *string = info[i].data.string.source;
			int pos = info[i].data.string.length - 1;

			assert(info[i].type == SCAN_STRING && pos >= 0);

			for (; pos >= 0; pos--)
				scan_table[string[pos]] |= info[i].bits;
		}
	}
}

void
init_scanner(struct scanner *scanner, struct scanner_info *scanner_info,
	     unsigned char *string, unsigned char *end)
{
	if (!scanner_info->initialized) {
		init_scanner_info(scanner_info);
		scanner_info->initialized = 1;
	}

	memset(scanner, 0, sizeof(*scanner));

	scanner->string = string;
	scanner->position = string;
	scanner->end = end ? end : string + strlen(string);
	scanner->current = scanner->table;
	scanner->info = scanner_info;
	scanner->info->scan(scanner);
}

/** @} */
