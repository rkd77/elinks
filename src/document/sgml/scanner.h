
#ifndef EL__DOCUMENT_SGML_SCANNER_H
#define EL__DOCUMENT_SGML_SCANNER_H

#include "util/scanner.h"

enum sgml_token_type {
	/* Char tokens: */

	/* Char tokens range from 1 to 255 and have their char value as type */
	/* meaning non char tokens have values from 256 and up. */

	/* Low level string tokens: */

	SGML_TOKEN_IDENT = 256,		/* [0-9a-zA-Z_-:.]+ */
	SGML_TOKEN_TAG_END,		/* > or ?> */
	SGML_TOKEN_STRING,		/* Char sequence delimted by matching ' or " */

	/* High level string tokens: */

	SGML_TOKEN_NOTATION,		/* <!{ident} until > */
	SGML_TOKEN_NOTATION_COMMENT,	/* <!-- until --> */
	SGML_TOKEN_NOTATION_DOCTYPE,	/* <!DOCTYPE until > */
	SGML_TOKEN_NOTATION_ELEMENT,	/* <!ELEMENT until > */
	SGML_TOKEN_NOTATION_ENTITY,	/* <!ENTITY  until > */
	SGML_TOKEN_NOTATION_ATTLIST,	/* <!ATTLIST until > */

	SGML_TOKEN_PROCESS,		/* <?{ident} until ?> */
	SGML_TOKEN_PROCESS_XML,		/* <?xml until */

	SGML_TOKEN_ELEMENT,		/* <{ident}> */
	SGML_TOKEN_ELEMENT_BEGIN,	/* <{ident} */
	SGML_TOKEN_ELEMENT_END,		/* </{ident}> or </> */
	SGML_TOKEN_ELEMENT_EMPTY_END,	/* /> */
	SGML_TOKEN_ATTRIBUTE,		/* [^>\t\r\n\f\v ]+ */

	SGML_TOKEN_ENTITY,		/* &ident; */

	SGML_TOKEN_TEXT,		/* [^<&]+ */
	SGML_TOKEN_SPACE,		/* [\t\r\n\f\v ]+ */

	/* Special tokens: */

	/* A special token for unrecognized strings */
	SGML_TOKEN_GARBAGE,

	/* Token type used internally when scanning to signal that the token
	 * should not be recorded in the scanners token table. */
	SGML_TOKEN_SKIP,

	/* Another internal token type used both to mark unused tokens in the
	 * scanner table as invalid or when scanning to signal that the
	 * scanning should end. */
	SGML_TOKEN_NONE = 0,
};

extern struct scanner_info sgml_scanner_info;

/* Treat '<' as more valuable then '>' so that scanning of '<a<b>' using
 * skipping to next '>' will stop at the second '<'. */
#define get_sgml_precedence(token_type) \
	((token_type) == '<' ? (1 << 11) : \
	 (token_type) == '>' ? (1 << 10) : 0)

#define skip_sgml_tokens(scanner, type) \
	skip_scanner_tokens(scanner, type, get_sgml_precedence(type))

#endif
