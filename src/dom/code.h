#ifndef EL_DOM_CODE_H
#define EL_DOM_CODE_H

/** DOM status, error, and exception codes
 *
 * These enum values are used for return codes throughout the DOM engine.
 */
enum dom_code {
	/* ELinks specific codes: */
	DOM_CODE_OK = 0,		/**< The sane default. */
	DOM_CODE_ERR = -1000,		/**< Anything but #DOM_CODE_OK. */

	DOM_CODE_INCOMPLETE,		/**< The parsing could not be completed */
	DOM_CODE_FREE_NODE,		/**< Discard the node */

	DOM_CODE_ALLOC_ERR,		/**< Failed to allocate memory */
	DOM_CODE_MAX_DEPTH_ERR,		/**< Stack max depth reached */
	DOM_CODE_VALUE_ERR,		/**< Bad/unexpected value */

	/* DOM Level 1 codes: */

	/** Index or size is negative, or greater than the allowed
	 * value.*/
	DOM_CODE_INDEX_SIZE_ERR			=  1,
	/** A specified range of text does not fit into a DOMString. */
	DOM_CODE_DOMSTRING_SIZE_ERR		=  2,
	/** A node is inserted somewhere it doesn't belong. */
	DOM_CODE_HIERARCHY_REQUEST_ERR		=  3,
	/** A node is used in a different document than the one that
	 * created it (that doesn't support it). */
	DOM_CODE_WRONG_DOCUMENT_ERR		=  4,
	/** An invalid or illegal character is specified, such as in an
	 * XML name. */
	DOM_CODE_INVALID_CHARACTER_ERR		=  5,
	/** Data is specified for a node which does not support data. */
	DOM_CODE_NO_DATA_ALLOWED_ERR		=  6,
	/** An attempt is made to modify an object where modifications
	 * are not allowed. */
	DOM_CODE_NO_MODIFICATION_ALLOWED_ERR	=  7,
	/** An attempt is made to reference a node in a context where it
	 * does not exist. */
	DOM_CODE_NOT_FOUND_ERR			=  8,
	/** The implementation does not support the requested type of
	 * object or operation. */
	DOM_CODE_NOT_SUPPORTED_ERR		=  9,
	/** An attempt is made to add an attribute that is already in
	 * use elsewhere. */
	DOM_CODE_INUSE_ATTRIBUTE_ERR		= 10,

	/* Introduced in DOM Level 2: */

	/** An attempt is made to use an object that is not, or is no
	 * longer, usable. */
	DOM_CODE_INVALID_STATE_ERR		= 11,
	/** An invalid or illegal string is specified. */
	DOM_CODE_SYNTAX_ERR			= 12,
	/** An attempt is made to modify the type of the underlying
	 * object. */
	DOM_CODE_INVALID_MODIFICATION_ERR	= 13,
	/** An attempt is made to create or change an object in a way
	 * which is incorrect with regard to namespaces. */
	DOM_CODE_NAMESPACE_ERR			= 14,
	/** A parameter or an operation is not supported by the
	 * underlying object. */
	DOM_CODE_INVALID_ACCESS_ERR		= 15,

	/* Introduced in DOM Level 3: */

	/** A call to a method such as insertBefore or removeChild would
	 * make the Node invalid with respect to "partial validity",
	 * this exception would beraised and the operation would not be
	 * done. This code is used in DOM Level 3 Validation. */
	DOM_CODE_VALIDATION_ERR			= 16,
	/** A type of an object is incompatible with the expected type
	 * of the parameter associated to the object. */
	DOM_CODE_TYPE_MISMATCH_ERR		= 17,
};

#endif
