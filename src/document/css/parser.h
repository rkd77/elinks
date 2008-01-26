/*! @file
 * This is interface for the value parser. It is intended to be used
 * only internally inside of the CSS engine. */

#ifndef EL__DOCUMENT_CSS_PARSER_H
#define EL__DOCUMENT_CSS_PARSER_H

#include "util/lists.h"
struct scanner;
struct css_stylesheet;
struct uri;

/** This function takes a semicolon separated list of declarations
 * from the given string, parses them to atoms, and chains the newly
 * created struct css_property objects to the specified list.
 * @returns positive value in case it recognized a property in the
 * given string, or zero in case of an error. */
void css_parse_properties(LIST_OF(struct css_property) *props,
			  struct scanner *scanner);


/** Parses the @a string and adds any recognized selectors + properties to the
 * given stylesheet @a css. If the selector is already in the stylesheet it
 * properties are added to the that selector. */
void css_parse_stylesheet(struct css_stylesheet *css, struct uri *base_uri,
			  const unsigned char *string, const unsigned char *end);

#endif
