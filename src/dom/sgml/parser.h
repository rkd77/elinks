#ifndef EL_DOM_SGML_PARSER_H
#define EL_DOM_SGML_PARSER_H

#include "dom/code.h"
#include "dom/node.h"
#include "dom/stack.h"
#include "dom/sgml/sgml.h"
#include "dom/scanner.h"

struct sgml_parser;
struct string;
struct uri;

/** SGML parser type
 *
 * There are two kinds of parser types: One that optimises one-time access to
 * the DOM tree and one that creates a persistent DOM tree. */
enum sgml_parser_type {
	/**
	 * The first one will simply push nodes on the stack, not building a
	 * DOM tree. This interface is similar to that of SAX (Simple API for
	 * XML) where events are fired when nodes are entered and exited. It is
	 * useful when you are not actually interested in the DOM tree, but can
	 * do all processing in a stream-like manner, such as when highlighting
	 * HTML code. */
	SGML_PARSER_STREAM,
	/**
	 * The second one is a DOM tree builder, that builds a persistent DOM
	 * tree. When using this type, it is possible to do even more
	 * (pre)processing than for parser streams. For example you can sort
	 * element child nodes, or purge various node such as text nodes that
	 * only contain space characters.  */
	SGML_PARSER_TREE,
};

/** SGML parser flags
 *
 * These flags control how the parser behaves.
 */
enum sgml_parser_flag {
	SGML_PARSER_COUNT_LINES	= 1,	/**< Make line numbers available. */
	SGML_PARSER_COMPLETE = 2,	/**< Used internally when incremental. */
	SGML_PARSER_INCREMENTAL = 4,	/**< Parse chunks of input. */
	SGML_PARSER_DETECT_ERRORS = 8,	/**< Report errors. */
};

/** SGML parser state
 *
 * The SGML parser has only little state.
 */
struct sgml_parser_state {
	/**
	 * Info about the properties of the node contained by state.
	 * This is only meaningful to element and attribute nodes. For
	 * unknown nodes it points to the common 'unknown node' info. */
	struct sgml_node_info *info;
	/**
	 * This is used by the DOM source renderer for highlighting the
	 * end-tag of an element. */
	struct dom_scanner_token end_token;
};

/** SGML error callback
 *
 * Called by the SGML parser when a parsing error has occurred.
 *
 * If the return code is not #DOM_CODE_OK the parsing will be
 * ended and that code will be returned. */
typedef enum dom_code
(*sgml_error_T)(struct sgml_parser *, struct dom_string *, unsigned int);


/** The SGML parser
 *
 * This struct hold info used while parsing SGML data.
 *
 * @note	The only variable the user should set is
 *		sgml_parser.error_func.
 */
struct sgml_parser {
	enum sgml_parser_type type;	/**< Stream or tree */
	enum sgml_parser_flag flags;	/**< Flags that control the behaviour */

	struct sgml_info *info;		/**< Backend dependent info */

	struct dom_string uri;		/**< The URI of the DOM document */
	struct dom_node *root;		/**< The document root node */

	enum dom_code code;		/**< The latest (error) code */
	sgml_error_T error_func;	/**< Called for detected errors */

	struct dom_stack stack;		/**< A stack for tracking parsed nodes */
	struct dom_stack parsing;	/**< Used for tracking parsing states */
};


/** Initialise an SGML parser
 *
 * Initialise an SGML parser with the given properties.
 *
 * @param type		Stream or tree; one-time or persistant.
 * @param doctype	The document type, this affects what sub type nodes are
 * 			given.
 * @param uri		The URI of the document root.
 * @param flags		Flags controlling the behaviour of the parser.
 *
 * @returns		The created parser or NULL.
 */
struct sgml_parser *
init_sgml_parser(enum sgml_parser_type type, enum sgml_document_type doctype,
		 struct dom_string *uri, enum sgml_parser_flag flags);

/** Release an SGML parser
 *
 * Deallocates all resources, except the root node.
 *
 * @param parser	The parser being released.
 */
void done_sgml_parser(struct sgml_parser *parser);

/** Parse a chunk of SGML source
 *
 * Parses the given buffer. For incremental rendering the last buffer can be
 * signals through the `complete` parameter.
 *
 * @param parser	A parser created with #init_sgml_parser.
 * @param buf		A buffer containing the chunk to parse.
 * @param bufsize	The size of the buffer given in the buf parameter.
 * @param complete	Whether this is the last chunk to parse.
 *
 * @returns		#DOM_CODE_OK if the buffer was successfully parsed,
 *			else a code hinting at the error.
 */
enum dom_code
parse_sgml(struct sgml_parser *parser, unsigned char *buf, size_t bufsize, int complete);

/** Get the line position in the source
 *
 * @note		Line numbers are recorded in the scanner tokens.
 *
 * @param parser	A parser created with #init_sgml_parser.
 * @returns		What line number the parser is currently at or zero if
 *			there has been no parsing yet.
 */
unsigned int get_sgml_parser_line_number(struct sgml_parser *parser);

#endif
