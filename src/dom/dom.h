#ifndef EL_DOM_DOM_H
#define EL_DOM_DOM_H

/** DOM status/error/exception codes
 *
 * These enum values are used for return codes throughout the DOM engine.
 */
enum dom_code {
	/** ELinks specific codes: */
	DOM_CODE_OK = 0,		/*: The sane default. */
	DOM_CODE_ERR = -1000,		/*: Anything by DOM_CODE_OK. */

	DOM_CODE_INCOMPLETE,		/*: The parsing could not be completed */
	DOM_CODE_FREE_NODE,		/*: Discard the node */

	/** Error codes: */
	DOM_CODE_ALLOC_ERR,		/*: Failed to allocate memory */
	DOM_CODE_MAX_DEPTH_ERR,		/*: Stack max depth reached */
	DOM_CODE_VALUE_ERR,		/*: Bad/unexpected value */

	/** DOM Level 1 codes: */
	DOM_CODE_INDEX_SIZE_ERR			=  1,
	DOM_CODE_STRING_SIZE_ERR		=  2,
	DOM_CODE_HIERARCHY_REQUEST_ERR		=  3,
	DOM_CODE_WRONG_DOCUMENT_ERR		=  4,
	DOM_CODE_INVALID_CHARACTER_ERR		=  5,
	DOM_CODE_NO_DATA_ALLOWED_ERR		=  6,
	DOM_CODE_NO_MODIFICATION_ALLOWED_ERR	=  7,
	DOM_CODE_NOT_FOUND_ERR			=  8,
	DOM_CODE_NOT_SUPPORTED_ERR		=  9,
	DOM_CODE_INUSE_ATTRIBUTE_ERR		= 10,

	/** Introduced in DOM Level 2: */
	DOM_CODE_INVALID_STATE_ERR		= 11,
	DOM_CODE_SYNTAX_ERR			= 12,
	DOM_CODE_INVALID_MODIFICATION_ERR	= 13,
	DOM_CODE_NAMESPACE_ERR			= 14,
	DOM_CODE_INVALID_ACCESS_ERR		= 15,

	/** Introduced in DOM Level 3: */
	DOM_CODE_VALIDATION_ERR			= 16,
	DOM_CODE_TYPE_MISMATCH_ERR		= 17,
};

#endif
