/*! @file
 * This is interface for the value parser. It is intended to be used
 * only internally inside of the CSS engine. */

#ifndef EL__DOCUMENT_CSS_VALUE_H
#define EL__DOCUMENT_CSS_VALUE_H

#include "document/css/property.h"
#include "document/css/scanner.h"


/** This function takes a value of a specified type from the given
 * scanner and converts it to a reasonable struct css_property-ready
 * form.
 *
 * It returns positive integer upon success, zero upon parse error,
 * and moves the string pointer to the byte after the value end. */
int css_parse_value(struct css_property_info *propinfo,
		    union css_property_value *value,
		    struct scanner *scanner);


/* Here come the css_property_value_parsers provided. */

/*! Takes no parser_data. */
int css_parse_background_value(struct css_property_info *propinfo,
				union css_property_value *value,
				struct scanner *scanner);

/*! Takes no parser_data. */
int css_parse_color_value(struct css_property_info *propinfo,
			  union css_property_value *value,
			  struct scanner *scanner);

/*! Takes no parser_data. */
int css_parse_display_value(struct css_property_info *propinfo,
			    union css_property_value *value,
			    struct scanner *scanner);

/*! Takes no parser_data. */
int css_parse_text_decoration_value(struct css_property_info *propinfo,
				union css_property_value *value,
				struct scanner *scanner);

/*! Takes no parser_data. */
int css_parse_font_style_value(struct css_property_info *propinfo,
				union css_property_value *value,
				struct scanner *scanner);

/*! Takes no parser_data. */
int css_parse_font_weight_value(struct css_property_info *propinfo,
				union css_property_value *value,
				struct scanner *scanner);
/*! Takes no parser_data. */
int css_parse_list_style_value(struct css_property_info *propinfo,
                               union css_property_value *value,
                               struct scanner *scanner);

/*! Takes no parser_data. */
int css_parse_text_align_value(struct css_property_info *propinfo,
				union css_property_value *value,
				struct scanner *scanner);

/*! Takes no parser_data. */
int css_parse_white_space_value(struct css_property_info *propinfo,
				union css_property_value *value,
				struct scanner *scanner);
#endif
