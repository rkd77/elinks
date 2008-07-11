#ifndef EL__DOM_CONFIGURATION_H
#define EL__DOM_CONFIGURATION_H

struct dom_node;
struct dom_stack;

/** DOM Configuration
 *
 * The DOMConfiguration interface represents the configuration of a document.
 * Using the configuration, it is possible to change the behaviour of how
 * document normalization is done, such as replacing the CDATASection nodes
 * with Text nodes.
 *
 * Note: Parameters are similar to features and properties used in SAX2.
 */

/** DOM configuration flags.
 *
 * The following list of parameters defined in the DOM: */
enum dom_config_flag {
	/** "cdata-sections"
	 *
	 * The default is true and will keep CDATASection nodes in the
	 * document. When false, CDATASection nodes in the document are
	 * transformed into Text nodes. The new Text node is then combined with
	 * any adjacent Text node. */
	DOM_CONFIG_CDATA_SECTIONS = 1,

	/** "comments"
	 *
	 * If true (the default) keep Comment nodes in the document, else
	 * discard them. */
	DOM_CONFIG_COMMENTS = 2,

	/** "element-content-whitespace"
	 *
	 * The default is true and will keep all whitespaces in the document.
	 * When false, discard all Text nodes that contain only whitespaces. */
	DOM_CONFIG_ELEMENT_CONTENT_WHITESPACE = 4,

	/** "entities"
	 *
	 * When true (the default) keep EntityReference nodes in the document.
	 * When false, remove all EntityReference nodes from the document,
	 * putting the entity expansions directly in their place. Text nodes
	 * are normalized. Only unexpanded entity references are kept in the
	 * document. Note: This parameter does not affect Entity nodes. */
	DOM_CONFIG_ENTITIES = 8,

	/** "normalize-characters"
	 *
	 * The default is false, not to perform character normalization, else
	 * fully normalized the characters in the document as defined in
	 * appendix B of [XML 1.1]. */
	DOM_CONFIG_NORMALIZE_CHARACTERS = 16,

	/** "unknown"
	 *
	 * If false (default) nothing is done, else elements and attributes
	 * that are not known according to the built-in node info are
	 * discarded. */
	DOM_CONFIG_UNKNOWN = 32,

	/** "normalize-whitespace"
	 *
	 * If false (default) nothing is done, else all text nodes are
	 * normalized so that sequences of space characters are changed to
	 * being only a single space. */
	DOM_CONFIG_NORMALIZE_WHITESPACE = 64,
};

struct dom_error;

struct dom_config {
	enum dom_config_flag flags; /**< DOM configuration flags. */

	/** A user defined error handler.
	 *
	 * Contains an error handler. If an error is encountered in the
	 * document, this handler is called. When called, DOMError.relatedData
	 * will contain the closest node to where the error occurred. If the
	 * implementation is unable to determine the node where the error
	 * occurs, DOMError.relatedData will contain the Document node.
	 */
	void (*error_handler)(struct dom_config *, struct dom_error *);
};

struct dom_config *
add_dom_config_normalizer(struct dom_stack *stack, struct dom_config *config,
			  enum dom_config_flag flags);

enum dom_config_flag
parse_dom_config(unsigned char *flaglist, unsigned char separator);

#endif
