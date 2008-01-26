/* BitTorrent bencoding scanner and parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h> /* OS/2 needs this after sys/types.h */
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "protocol/bittorrent/bencoding.h"
#include "protocol/bittorrent/common.h"
#include "util/error.h"
#include "util/sha1.h"
#include "util/scanner.h"
#include "util/string.h"
#include "util/time.h"


/* The various token types and what they contain. */
enum bencoding_token {
	/* Char tokens: */

	/* Char tokens range from 1 to 255 and have their char value as type */
	/* meaning non char tokens have values from 256 and up. */

	BENCODING_TOKEN_INTEGER		 = 'i',	/* 'i' <integer> 'e' */
	BENCODING_TOKEN_LIST		 = 'l',	/* 'l' ( <any> ) * 'e' */
	BENCODING_TOKEN_DICTIONARY	 = 'd',	/* 'd' ( <string> <any> ) * 'e' */
	BENCODING_TOKEN_END		 = 'e',


	/* Low level tokens: */

	BENCODING_TOKEN_STRING		 = 256,	/* <integer> ':' <bytes> */

	/* High level tokens: */

	/* Common tokens. */
	BENCODING_TOKEN_FILES,			/* dictionary */
	BENCODING_TOKEN_NAME,			/* string */

	/* Tokens related to the metainfo file. */
	BENCODING_TOKEN_ANNOUNCE,		/* string */
	BENCODING_TOKEN_ANNOUNCE_LIST,		/* list */
	BENCODING_TOKEN_COMMENT,		/* string */
	BENCODING_TOKEN_CREATED_BY,		/* string */
	BENCODING_TOKEN_CREATION_DATE,		/* integer */
	BENCODING_TOKEN_INFO,			/* dictionary */
	BENCODING_TOKEN_LENGTH,			/* integer */
	BENCODING_TOKEN_MD5SUM,			/* string */
	BENCODING_TOKEN_PATH,			/* string */
	BENCODING_TOKEN_PIECES,			/* string */
	BENCODING_TOKEN_PIECE_LENGTH,		/* integer */

	/* Tokens related to the tracker protocol response. */
	BENCODING_TOKEN_FAILURE_REASON,		/* string */
	BENCODING_TOKEN_INTERVAL,		/* integer */
	BENCODING_TOKEN_IP,			/* string */
	BENCODING_TOKEN_PEERS,			/* dictionary */
	BENCODING_TOKEN_PEER_ID,		/* string */
	BENCODING_TOKEN_PORT,			/* integer */

	/* Tokens related to the tracker `scrape' response. */
	BENCODING_TOKEN_COMPLETE,		/* integer */
	BENCODING_TOKEN_DOWNLOADED,		/* integer */
	BENCODING_TOKEN_INCOMPLETE,		/* integer */

	/* Another internal token type used both to mark unused tokens in the
	 * scanner table as invalid or when scanning to signal that the
	 * scanning should end. */
	BENCODING_TOKEN_NONE = 0,

	/* Special token to report syntax errors. */
	BENCODING_TOKEN_ERROR = 1,
};


/* ************************************************************************** */
/* The scanner: */
/* ************************************************************************** */

#define	is_bencoding_integer(c)		((c) == BENCODING_TOKEN_INTEGER)
#define	is_bencoding_list(c)		((c) == BENCODING_TOKEN_LIST)
#define	is_bencoding_dictionary(c)	((c) == BENCODING_TOKEN_DICTIONARY)
#define	is_bencoding_end(c)		((c) == BENCODING_TOKEN_END)
#define	is_bencoding_string(c)		(isdigit(c))

#define	scan_bencoding_integer(scanner, s)	\
	while ((s) < (scanner)->end && isdigit(*(s))) (s)++;

static inline void
scan_bencoding_token(struct scanner *scanner, struct scanner_token *token)
{
	unsigned char *string = scanner->position;
	unsigned char first_char = *string;
	enum bencoding_token type = BENCODING_TOKEN_NONE;
	int real_length = -1;

	token->string = string++;

	if (is_bencoding_string(first_char)) {
		unsigned long string_length;

		scan_bencoding_integer(scanner, string);

		/* Check the length delimiter. */
		if (*string == ':') {
			errno = 0;
			string_length = strtoul(token->string, NULL, 10);
			if (!errno
			    && string_length < INT_MAX
			    && string + string_length < scanner->end) {
				/* Claim the string data. */
				token->string = string + 1;
				real_length   = string_length;
				string	      = token->string + string_length;
				type = BENCODING_TOKEN_STRING;
			}
		}

	} else if (is_bencoding_end(first_char)) {
		type = BENCODING_TOKEN_END;

	} else if (is_bencoding_integer(first_char)) {
		unsigned char *integer_start = string;

		/* Signedness. */
		if (*string == '-') string++;

		/* Scan to the end marker */
		scan_bencoding_integer(scanner, string);

		if (*string == BENCODING_TOKEN_END) {
			token->string = integer_start;
			real_length   = string - integer_start;
			type = BENCODING_TOKEN_INTEGER;
			string++;
		}

	} else if (is_bencoding_dictionary(first_char)) {
		type = BENCODING_TOKEN_DICTIONARY;

	} else if (is_bencoding_list(first_char)) {
		type = BENCODING_TOKEN_LIST;
	}

	token->type = type;
	token->length = real_length > 0 ? real_length : string - token->string;
	scanner->position = string;
}

static void
skip_bencoding_tokens(struct scanner *scanner)
{
	int nesting_level = 0;

	assert(scanner_has_tokens(scanner));

	/* Skip while nesting_level is > 0 since the first token can be the end
	 * token. */
	do {
		struct scanner_token *token = get_scanner_token(scanner);

		if (!token) return;

		switch (token->type) {
		case BENCODING_TOKEN_INTEGER:
		case BENCODING_TOKEN_STRING:
			break;

		case BENCODING_TOKEN_DICTIONARY:
		case BENCODING_TOKEN_LIST:
			nesting_level++;
			break;

		case BENCODING_TOKEN_END:
			nesting_level--;
			break;

		default:
			INTERNAL("Scanner error detected");
		}

		skip_scanner_token(scanner);

	} while (nesting_level > 0 && scanner_has_tokens(scanner));
}

static struct scanner_token *
scan_bencoding_tokens(struct scanner *scanner)
{
	struct scanner_token *table_end = scanner->table + SCANNER_TOKENS;
	struct scanner_token *current;

	if (!begin_token_scanning(scanner))
		return get_scanner_token(scanner);

	/* Scan tokens until we fill the table */
	for (current = scanner->table + scanner->tokens;
	     current < table_end && scanner->position < scanner->end;
	     current++) {
		if (scanner->position >= scanner->end) break;

		scan_bencoding_token(scanner, current);

		/* Did someone scream for us to end the madness? */
		if (current->type == BENCODING_TOKEN_NONE) {
			scanner->position = NULL;
			current--;
			break;
		}
	}

	return end_token_scanning(scanner, current);
}


struct scanner_info bencoding_scanner_info = {
	NULL,
	NULL,
	scan_bencoding_tokens,
};


/* ************************************************************************** */
/* BitTorrent specific dictionary value type checking: */
/* ************************************************************************** */

struct bencoding_dictionary_info {
	unsigned char *key;
	enum bencoding_token key_type;
	enum bencoding_token value_type;
};

#define DICT(key, keytype, valuetype) \
	{ key, BENCODING_TOKEN_##keytype, BENCODING_TOKEN_##valuetype }

static const struct bencoding_dictionary_info bencoding_dictionary_entries[] = {
	/*    <key>		<key-type>	<value-type> */
	DICT("announce list",	ANNOUNCE_LIST,	LIST),
	DICT("announce",	ANNOUNCE,	STRING),
	DICT("comment",		COMMENT,	STRING),
	DICT("complete",	COMPLETE,	INTEGER),
	DICT("created by",	CREATED_BY,	STRING),
	DICT("creation date",	CREATION_DATE,	INTEGER),
	DICT("downloaded",	DOWNLOADED,	INTEGER),
	DICT("failure reason",	FAILURE_REASON,	STRING),
	DICT("files",		FILES,		LIST),
	DICT("incomplete",	INCOMPLETE,	INTEGER),
	DICT("info",		INFO,		DICTIONARY),
	DICT("interval",	INTERVAL,	INTEGER),
	DICT("ip",		IP,		STRING),
	DICT("length",		LENGTH,		INTEGER),
	DICT("md5sum",		MD5SUM,		STRING),
	DICT("name",		NAME,		STRING),
	DICT("path",		PATH,		LIST),
	DICT("peer id",		PEER_ID,	STRING),
	DICT("peers",		PEERS,		LIST),
	DICT("peers",		PEERS,		STRING),
	DICT("piece length",	PIECE_LENGTH,	INTEGER),
	DICT("pieces",		PIECES,		STRING),
	DICT("port",		PORT,		INTEGER),

	DICT(NULL,		NONE,		NONE),
};

#undef DICT

/* Looks up the key type and validates that the value token is valid. */
enum bencoding_token
check_bencoding_dictionary_entry(struct scanner *scanner,
				 struct scanner_token **value_ptr)
{
	const struct bencoding_dictionary_info *entry;
	struct scanner_token *key, *value, key_backup;

	key = get_scanner_token(scanner);
	if (!key) return BENCODING_TOKEN_ERROR;

	if (key->type == BENCODING_TOKEN_END)
		return BENCODING_TOKEN_END;

	if (key->type != BENCODING_TOKEN_STRING)
		return BENCODING_TOKEN_NONE;

	/* Backup the token since the it might disappear when requesting the
	 * following value token. */
	copy_struct(&key_backup, key);
	key = &key_backup;

	*value_ptr = value = get_next_scanner_token(scanner);
	if (!value) return BENCODING_TOKEN_ERROR;

	for (entry = bencoding_dictionary_entries; entry->key; entry++) {
		if (!scanner_token_strlcasecmp(key, entry->key, -1))
			continue;

		/* Type-check the value. Some keys have multiple types. */
		if (value->type != entry->value_type)
			continue;

		return entry->key_type;
	}

	return BENCODING_TOKEN_NONE;
}


/* ************************************************************************** */
/* The .torrent metafile parsing: */
/* ************************************************************************** */

static off_t
parse_bencoding_integer(struct scanner_token *token)
{
	const unsigned char *string = token->string;
	int pos = 0, length = token->length;
	off_t integer = 0;
	int sign = 1;

	assert(length);

	if (string[pos] == '-') {
		pos++;
		sign = -1;
	}

	for (; pos < length && isdigit(string[pos]); pos++) {
		off_t newint = integer * 10 + string[pos] - '0';

		/* Check for overflow.  This assumes wraparound,
		 * even though C does not guarantee that.  */
		if (newint / 10 != integer)
			return 0;
		integer = newint;
	}

	if (sign == -1)
		integer *= sign;

	return integer;
}

static unsigned char *
normalize_bencoding_path(const unsigned char *path, int pathlen,
			 int *malicious)
{
	struct string string;

	/* Normalize and check for malicious paths in the the file list. */

	if (!init_string(&string)
	    || !add_to_string(&string, "file://./")
	    || !add_bytes_to_string(&string, path, pathlen)) {
		done_string(&string);
		return NULL;
	}

	path = normalize_uri(NULL, string.source);

	/* This shouldn't happened but it makes sense to be a little paranoid
	 * here. ;-) */
	if (memcmp(path, "file://", 7)) {
		done_string(&string);
		return NULL;
	}

	/* The normalization will make the path start with './' if the path is
	 * OK and '/' if the path contained directory elevators which moved
	 * outside the current working directory (CWD). These potentially
	 * malicous paths will be translated to just be nested in the CWD. */
	*malicious = !!dir_sep(path[7]);

	path += 8 + !*malicious;
	memmove(string.source, path, strlen(path) + 1);

	return string.source;
}

/* Add file to the file list. The new file is based on info from the passed
 * template and will have the given path after it has been normalized and
 * checked for sanity. */
static enum bittorrent_state
add_bittorrent_file(struct bittorrent_meta *meta, unsigned char *path,
		    struct bittorrent_file *template)
{
	struct bittorrent_file *file;
	int malicious;
	int pathlen;

	/* Normalize and check for malicious paths in the the file list. */
	path = normalize_bencoding_path(path, strlen(path), &malicious);
	if (!path) return BITTORRENT_STATE_OUT_OF_MEM;

	if (malicious)
		meta->malicious_paths = malicious;

	pathlen = strlen(path);

	file = mem_calloc(1, sizeof(*file) + pathlen);
	if (!file) {
		mem_free(path);
		return BITTORRENT_STATE_OUT_OF_MEM;
	}

	copy_struct(file, template);
	memcpy(file->name, path, pathlen);
	mem_free(path);

	file->selected = 1;

	add_to_list_end(meta->files, file);

	return BITTORRENT_STATE_OK;
}

/* Parses a list of path elements and adds them each to the path string
 * separated by the platform specific directory separater. */
static enum bittorrent_state
parse_bencoding_file_path(struct scanner *scanner, struct string *path)
{
	assert(get_scanner_token(scanner)->type == BENCODING_TOKEN_LIST);

	skip_scanner_token(scanner);

	while (scanner_has_tokens(scanner)) {
		struct scanner_token *token = get_scanner_token(scanner);

		if (!token) break;

		if (token->type == BENCODING_TOKEN_END) {
			return BITTORRENT_STATE_OK;
		}

		if (token->type != BENCODING_TOKEN_STRING)
			break;

		if (path->length > 0) {
			/* Somewhat platform independant. dir_sep() is either a
			 * macro or an inline function so the compiler should
			 * optimize away the unneded branch. */
			unsigned char separator = dir_sep('\\') ? '\\' : '/';

			add_char_to_string(path, separator);
		}

		add_bytes_to_string(path, token->string, token->length);
		skip_scanner_token(scanner);
	}

	return BITTORRENT_STATE_ERROR;
}

/* Parse a dictionary of file information used for multi-file torrents. */
static enum bittorrent_state
parse_bencoding_file_dictionary(struct bittorrent_meta *meta,
				struct scanner *scanner, struct string *path)
{
	struct bittorrent_file file;

	assert(get_scanner_token(scanner)->type == BENCODING_TOKEN_DICTIONARY);

	skip_scanner_token(scanner);

	memset(&file, 0, sizeof(file));

	while (scanner_has_tokens(scanner)) {
		struct scanner_token *value;
		enum bittorrent_state state;

		switch (check_bencoding_dictionary_entry(scanner, &value)) {
		case BENCODING_TOKEN_PATH:
			state = parse_bencoding_file_path(scanner, path);
			if (state != BITTORRENT_STATE_OK)
				return state;
			skip_scanner_token(scanner);
			break;

		case BENCODING_TOKEN_LENGTH:
			file.length = parse_bencoding_integer(value);
			skip_scanner_token(scanner);
			break;

		case BENCODING_TOKEN_MD5SUM:
			if (value->length != sizeof(md5_digest_hex_T))
				return BITTORRENT_STATE_ERROR;

			memcpy(file.md5sum, value->string, value->length);
			skip_scanner_token(scanner);
			break;

		case BENCODING_TOKEN_END:
			skip_scanner_token(scanner);
			return add_bittorrent_file(meta, path->source, &file);

		case BENCODING_TOKEN_ERROR:
			return BITTORRENT_STATE_ERROR;

		case BENCODING_TOKEN_NONE:
		default:
			skip_bencoding_tokens(scanner);
		}
	}

	return BITTORRENT_STATE_ERROR;
}

/* Parse a list of file dictionaries. */
static enum bittorrent_state
parse_bencoding_files_list(struct bittorrent_meta *meta, struct scanner *scanner)
{
	assert(get_scanner_token(scanner)->type == BENCODING_TOKEN_LIST);

	skip_scanner_token(scanner);

	while (scanner_has_tokens(scanner)) {
		struct scanner_token *token = get_scanner_token(scanner);
		struct string path;
		enum bittorrent_state state;

		if (!token) break;

		if (token->type == BENCODING_TOKEN_END) {
			skip_scanner_token(scanner);
			return BITTORRENT_STATE_OK;
		}

		if (token->type != BENCODING_TOKEN_DICTIONARY)
			return BITTORRENT_STATE_ERROR;

		/* Allocating and freeing the path string here makes error
		 * handling so much easier in parse_bencoding_file_dictionary()
		 * because it can return right away. */
		if (!init_string(&path))
			return BITTORRENT_STATE_OUT_OF_MEM;

		state = parse_bencoding_file_dictionary(meta, scanner, &path);
		done_string(&path);
		if (state != BITTORRENT_STATE_OK)
			return state;
	}

	return BITTORRENT_STATE_ERROR;
}

/* Parse the info dictionary which contains file and piece infomation. */
static enum bittorrent_state
parse_bencoding_info_dictionary(struct bittorrent_meta *meta,
				struct scanner *scanner)
{
	struct bittorrent_file file;

	assert(get_scanner_token(scanner)->type == BENCODING_TOKEN_DICTIONARY);

	skip_scanner_token(scanner);

	memset(&file, 0, sizeof(file));

	while (scanner_has_tokens(scanner)) {
		struct scanner_token *value;
		enum bittorrent_state state;
		int malicious;
		off_t length;

		switch (check_bencoding_dictionary_entry(scanner, &value)) {
		case BENCODING_TOKEN_NAME:
			meta->name = normalize_bencoding_path(value->string,
							      value->length,
							      &malicious);
			if (!meta->name) return BITTORRENT_STATE_OUT_OF_MEM;
			if (malicious)
				meta->malicious_paths = malicious;
			skip_scanner_token(scanner);
			break;

		case BENCODING_TOKEN_PIECES:
			/* The piece hash must be a multiple of the SHA digest
			 * length. */
			if ((value->length % SHA_DIGEST_LENGTH) != 0)
				return BITTORRENT_STATE_ERROR;

			meta->pieces = value->length / SHA_DIGEST_LENGTH;
			meta->piece_hash = memacpy(value->string, value->length);
			skip_scanner_token(scanner);
			break;

		case BENCODING_TOKEN_PIECE_LENGTH:
			length = parse_bencoding_integer(value);
			if (length < 0 || length >= INT_MAX)
				return BITTORRENT_STATE_ERROR;

			meta->piece_length = (uint32_t) length;
			skip_scanner_token(scanner);
			break;

		case BENCODING_TOKEN_FILES:
			meta->type = BITTORRENT_MULTI_FILE;
			state = parse_bencoding_files_list(meta, scanner);
			if (state != BITTORRENT_STATE_OK)
				return state;
			break;

		case BENCODING_TOKEN_LENGTH:
			file.length = parse_bencoding_integer(value);
			skip_scanner_token(scanner);
			break;

		case BENCODING_TOKEN_MD5SUM:
			if (value->length != 32)
				return BITTORRENT_STATE_ERROR;

			memcpy(file.md5sum, value->string, value->length);
			skip_scanner_token(scanner);
			break;

		case BENCODING_TOKEN_END:
			/* All file info was saved from the 'files' list. */
			if (meta->type == BITTORRENT_MULTI_FILE)
				return BITTORRENT_STATE_OK;

			if (!meta->name)
				return BITTORRENT_STATE_ERROR;

			return add_bittorrent_file(meta, meta->name, &file);

		case BENCODING_TOKEN_ERROR:
			return BITTORRENT_STATE_ERROR;

		case BENCODING_TOKEN_NONE:
		default:
			skip_bencoding_tokens(scanner);
		}
	}

	/* Check if all requirements were met. */
	return BITTORRENT_STATE_ERROR;
}

/* Validate that the bencoded metainfo file contained all the required fields
 * and that their values are sane. */
static enum bittorrent_state
check_bittorrent_metafile(struct bittorrent_meta *meta)
{
	struct bittorrent_file *file;
	off_t last_piece_length = 0;
	off_t total_length = 0;

	if (bittorrent_id_is_empty(meta->info_hash)
	    || !meta->pieces
	    || !meta->name
	    || !meta->name[0]
	    || !meta->piece_hash
	    || !meta->piece_length
	    || !meta->tracker_uris.size
	    || list_empty(meta->files))
		return BITTORRENT_STATE_ERROR;

	/* FIXME: Should we also check if any two files have the same name? */
	foreach (file, meta->files) {
		if (file->length < 0 || !file->name)
			return BITTORRENT_STATE_ERROR;

		total_length += file->length;
	}

	last_piece_length = (off_t) total_length % meta->piece_length;
	if (!last_piece_length)
		last_piece_length = meta->piece_length;

	meta->last_piece_length = (uint32_t) last_piece_length;

	/* Check that the non-zero last_piece_length can be stored. */
	if (meta->last_piece_length != last_piece_length)
		return BITTORRENT_STATE_ERROR;

	return BITTORRENT_STATE_OK;
}

enum bittorrent_state
parse_bittorrent_metafile(struct bittorrent_meta *meta, struct string *metafile)
{
	struct scanner scanner;

	memset(meta, 0, sizeof(*meta));
	init_list(meta->files);

	init_scanner(&scanner, &bencoding_scanner_info,
		     metafile->source, metafile->source + metafile->length);

	{
		struct scanner_token *token = get_scanner_token(&scanner);

		if (!token || token->type != BENCODING_TOKEN_DICTIONARY)
			return BITTORRENT_STATE_ERROR;

		skip_scanner_token(&scanner);
	}

	while (scanner_has_tokens(&scanner)) {
		struct scanner_token *value;

		switch (check_bencoding_dictionary_entry(&scanner, &value)) {
		case BENCODING_TOKEN_ANNOUNCE:
		{
			unsigned char *value_string;
			struct uri *uri;

			value_string = memacpy(value->string, value->length);
			skip_scanner_token(&scanner);

			if (!value_string) break;

			uri = get_uri(value_string, 0);
			mem_free(value_string);
			if (uri) {
				add_to_uri_list(&meta->tracker_uris, uri);
				done_uri(uri);
			}
			break;
		}
		case BENCODING_TOKEN_ANNOUNCE_LIST:
			/* FIXME: Add to the tracker URI list, when/if multiple
			 * trackers are/will be supported. */
			skip_bencoding_tokens(&scanner);
			break;

		case BENCODING_TOKEN_INFO:
		{
			const unsigned char *start = value->string;
			struct scanner_token *token;
			enum bittorrent_state state;

			state = parse_bencoding_info_dictionary(meta, &scanner);
			if (state != BITTORRENT_STATE_OK)
				return BITTORRENT_STATE_ERROR;

			token = get_scanner_token(&scanner);
			assert(token && token->type == BENCODING_TOKEN_END);

			/* Digest the dictionary to create the info hash. */
			SHA1(start, token->string + token->length - start,
			     meta->info_hash);

			skip_scanner_token(&scanner);
			break;
		}
		case BENCODING_TOKEN_COMMENT:
			meta->comment = memacpy(value->string,
						int_min(value->length, MAX_STR_LEN));
			skip_scanner_token(&scanner);
			break;

		case BENCODING_TOKEN_CREATION_DATE:
			/* Bug 923: Assumes time_t values fit in off_t.  */
			meta->creation_date = (time_t) parse_bencoding_integer(value);
			skip_scanner_token(&scanner);
			break;

		case BENCODING_TOKEN_CREATED_BY:
			skip_scanner_token(&scanner);
			break;

		case BENCODING_TOKEN_END:
			/* Check if all requirements were met. */
			return check_bittorrent_metafile(meta);

		case BENCODING_TOKEN_ERROR:
			return BITTORRENT_STATE_ERROR;

		case BENCODING_TOKEN_NONE:
		default:
			skip_bencoding_tokens(&scanner);
		}
	}

	return BITTORRENT_STATE_ERROR;
}


/* ************************************************************************** */
/* Tracker response parsing: */
/* ************************************************************************** */

static enum bittorrent_state
parse_bencoding_peer_dictionary(struct bittorrent_connection *bittorrent,
				struct scanner *scanner)
{
	struct scanner_token ip;
	bittorrent_id_T id;
	/* Set to invalid value. */
	int port = -1;

	assert(get_scanner_token(scanner)->type == BENCODING_TOKEN_DICTIONARY);

	skip_scanner_token(scanner);

	memset(id, 0, sizeof(bittorrent_id_T));
	memset(&ip, 0, sizeof(ip));

	while (scanner_has_tokens(scanner)) {
		struct scanner_token *value;

		switch (check_bencoding_dictionary_entry(scanner, &value)) {
		case BENCODING_TOKEN_IP:
			copy_struct(&ip, value);
			skip_scanner_token(scanner);
			break;

		case BENCODING_TOKEN_PORT:
			port = (int) parse_bencoding_integer(value);
			skip_scanner_token(scanner);
			break;

		case BENCODING_TOKEN_PEER_ID:
			if (value->length != sizeof(bittorrent_id_T))
				return BITTORRENT_STATE_ERROR;

			memcpy(id, value->string, value->length);
			skip_scanner_token(scanner);
			break;

		case BENCODING_TOKEN_END:
			skip_scanner_token(scanner);
			return add_peer_to_bittorrent_pool(bittorrent, id, port,
							   ip.string, ip.length);

		case BENCODING_TOKEN_ERROR:
			return BITTORRENT_STATE_ERROR;

		case BENCODING_TOKEN_NONE:
		default:
			skip_bencoding_tokens(scanner);
		}
	}

	return BITTORRENT_STATE_ERROR;
}

static enum bittorrent_state
parse_bencoding_peers_list(struct bittorrent_connection *bittorrent,
			   struct scanner *scanner)
{
	assert(get_scanner_token(scanner)->type == BENCODING_TOKEN_LIST);

	skip_scanner_token(scanner);

	while (scanner_has_tokens(scanner)) {
		struct scanner_token *token = get_scanner_token(scanner);
		enum bittorrent_state state;

		if (!token) break;

		if (token->type == BENCODING_TOKEN_END)
			return BITTORRENT_STATE_OK;

		if (token->type != BENCODING_TOKEN_DICTIONARY)
			return BITTORRENT_STATE_ERROR;

		state = parse_bencoding_peer_dictionary(bittorrent, scanner);
		if (state != BITTORRENT_STATE_OK)
			return state;
	}

	return BITTORRENT_STATE_ERROR;
}

/* Parses the compact peer list format. It is a string made up of substrings of
 * length 6, where the first 4 bytes hold the IP address and the 2 last bytes
 * hold the port number. */
static enum bittorrent_state
parse_bencoding_peers_string(struct bittorrent_connection *bittorrent,
			     struct scanner *scanner)
{
	struct scanner_token *token = get_scanner_token(scanner);
	const unsigned char *pos;
	const unsigned char *last_peer_info_start
		= token->string + token->length - 6;
	enum bittorrent_state state = BITTORRENT_STATE_OK;

	assert(get_scanner_token(scanner)->type == BENCODING_TOKEN_STRING);

	for (pos = token->string; pos <= last_peer_info_start; pos += 6) {
		/* Only IPv4 strings can occur in this format. */
		unsigned char ip[INET_ADDRSTRLEN];
		int iplen;
		uint16_t port;

		iplen = snprintf(ip, sizeof(ip), "%d.%d.%d.%d",
				(int) pos[0], (int) pos[1],
				(int) pos[2], (int) pos[3]);

		memcpy(&port, pos + 4, sizeof(port));
		port = ntohs(port);

		state = add_peer_to_bittorrent_pool(bittorrent, NULL, port,
						    ip, iplen);
		if (state != BITTORRENT_STATE_OK)
			break;
	}

	return state;
}

enum bittorrent_state
parse_bittorrent_tracker_response(struct bittorrent_connection *bittorrent,
				  struct string *response)
{
	struct scanner scanner;

	init_scanner(&scanner, &bencoding_scanner_info,
		     response->source, response->source + response->length);

	{
		struct scanner_token *token = get_scanner_token(&scanner);

		if (!token || token->type != BENCODING_TOKEN_DICTIONARY)
			return BITTORRENT_STATE_ERROR;

		skip_scanner_token(&scanner);
	}

	while (scanner_has_tokens(&scanner)) {
		struct scanner_token *value;
		enum bittorrent_state state;
		off_t integer;

		switch (check_bencoding_dictionary_entry(&scanner, &value)) {
		case BENCODING_TOKEN_FAILURE_REASON:
			response->source = value->string;
			response->length = value->length;

			return BITTORRENT_STATE_REQUEST_FAILURE;

		case BENCODING_TOKEN_INTERVAL:
			bittorrent->tracker.interval = (int) parse_bencoding_integer(value);
			skip_scanner_token(&scanner);
			break;

		case BENCODING_TOKEN_COMPLETE:
			integer = parse_bencoding_integer(value);
			if (0 < integer && integer < INT_MAX)
				bittorrent->complete = (uint32_t) integer;
			skip_scanner_token(&scanner);
			break;

		case BENCODING_TOKEN_INCOMPLETE:
			integer = parse_bencoding_integer(value);
			if (0 < integer && integer < INT_MAX)
				bittorrent->incomplete = (uint32_t) integer;
			skip_scanner_token(&scanner);
			break;

		case BENCODING_TOKEN_PEERS:
			/* There are two formats: the normal list and the more
			 * compact string variant. */
			switch (value->type) {
			case BENCODING_TOKEN_LIST:
				state = parse_bencoding_peers_list(bittorrent, &scanner);
				if (state != BITTORRENT_STATE_OK)
					return state;

				assert(get_scanner_token(&scanner)
				       && get_scanner_token(&scanner)->type
						       == BENCODING_TOKEN_END);
				break;

			case BENCODING_TOKEN_STRING:
				/* Parse peer list when using compact format. */
				state = parse_bencoding_peers_string(bittorrent, &scanner);
				if (state != BITTORRENT_STATE_OK)
					return state;
				assert(get_scanner_token(&scanner) == value);
				break;

			default:
				return BITTORRENT_STATE_ERROR;
			}

			skip_scanner_token(&scanner);
			break;

		case BENCODING_TOKEN_END:
			/* TODO: Check if all requirements were met. */
			return BITTORRENT_STATE_OK;

		case BENCODING_TOKEN_ERROR:
			return BITTORRENT_STATE_ERROR;

		case BENCODING_TOKEN_NONE:
		default:
			skip_bencoding_tokens(&scanner);
		}
	}

	return BITTORRENT_STATE_ERROR;
}
