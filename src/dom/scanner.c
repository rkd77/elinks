/* A pretty generic scanner */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include "elinks.h"

#include "dom/scanner.h"
#include "dom/string.h"
#include "util/error.h"


int
map_dom_scanner_string(struct dom_scanner *scanner,
		   unsigned char *ident, unsigned char *end, int base_type)
{
	const struct dom_scanner_string_mapping *mappings = scanner->info->mappings;
	struct dom_string name = INIT_DOM_STRING(ident, end - ident);

	for (; is_dom_string_set(&mappings->name); mappings++) {
		if (mappings->base_type == base_type
		    && !dom_string_casecmp(&mappings->name, &name))
			return mappings->type;
	}

	return base_type;
}


struct dom_scanner_token *
skip_dom_scanner_tokens(struct dom_scanner *scanner, int skipto, int precedence)
{
	struct dom_scanner_token *token = get_dom_scanner_token(scanner);

	/* Skip tokens while handling some basic precedens of special chars
	 * so we don't skip to long. */
	while (token) {
		if (token->type == skipto
		    || token->precedence > precedence)
			break;
		token = get_next_dom_scanner_token(scanner);
	}

	return (token && token->type == skipto)
		? get_next_dom_scanner_token(scanner) : NULL;
}

#ifdef DEBUG_SCANNER
void
dump_dom_scanner(struct dom_scanner *scanner)
{
	unsigned char buffer[MAX_STR_LEN];
	struct dom_scanner_token *token = scanner->current;
	struct dom_scanner_token *table_end = scanner->table + scanner->tokens;
	unsigned char *srcpos = token->string, *bufpos = buffer;
	int src_lookahead = 50;
	int token_lookahead = 4;
	int srclen;

	if (!dom_scanner_has_tokens(scanner)) return;

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

struct dom_scanner_token *
get_dom_scanner_token_debug(struct dom_scanner *scanner)
{
	if (!dom_scanner_has_tokens(scanner)) return NULL;

	dump_dom_scanner(scanner);

	/* Make sure we do not return invalid tokens */
	assert(!dom_scanner_has_tokens(scanner)
		|| scanner->current->type != 0);

	return get_dom_scanner_token(scanner);
}
#endif


/* Initializers */

static inline void
init_dom_scanner_info(struct dom_scanner_info *scanner_info)
{
	const struct dom_scan_table_info *info = scanner_info->scan_table_info;
	int *scan_table = scanner_info->scan_table;
	int i;

	if (!info) return;

	for (i = 0; info[i].type != DOM_SCAN_END; i++) {
		const struct dom_string *data = &info[i].data;

		if (info[i].type == DOM_SCAN_RANGE) {
			int index = *data->string;

			assert(index > 0);
			assert(data->length < DOM_SCAN_TABLE_SIZE);
			assert(index <= data->length);

			for (; index <= data->length; index++)
				scan_table[index] |= info[i].bits;

		} else {
			unsigned char *string = info[i].data.string;
			int pos = info[i].data.length - 1;

			assert(info[i].type == DOM_SCAN_STRING && pos >= 0);

			for (; pos >= 0; pos--)
				scan_table[string[pos]] |= info[i].bits;
		}
	}
}

void
init_dom_scanner(struct dom_scanner *scanner, struct dom_scanner_info *scanner_info,
		 struct dom_string *string, int state, int count_lines, int complete,
		 int check_complete, int detect_errors)
{
	if (!scanner_info->initialized) {
		init_dom_scanner_info(scanner_info);
		scanner_info->initialized = 1;
	}

	memset(scanner, 0, sizeof(*scanner));

	scanner->string = string->string;
	scanner->position = string->string;
	scanner->end = string->string + string->length;
	scanner->current = scanner->table;
	scanner->info = scanner_info;
	scanner->state = state;
	scanner->count_lines = !!count_lines;
	scanner->incomplete = !complete;
	scanner->check_complete = !!check_complete;
	scanner->detect_errors = !!detect_errors;
	scanner->lineno = scanner->count_lines;
	scanner->info->scan(scanner);
}
