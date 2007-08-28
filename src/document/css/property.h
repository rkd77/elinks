
#ifndef EL__DOCUMENT_CSS_PROPERTY_H
#define EL__DOCUMENT_CSS_PROPERTY_H

#include "document/format.h"
#include "util/align.h"
#include "util/color.h"
#include "util/lists.h"

/** The struct css_property describes one CSS declaration in a rule, therefore
 * being basically a parsed instance of struct css_property_info. One list of
 * these contains all the declarations contained in one rule. */
struct css_property {
	LIST_HEAD(struct css_property);

	/** Declared property. The enum item name is derived from the
	 * property name, just uppercase it and tr/-/_/. */
	enum css_property_type {
		CSS_PT_NONE,
		CSS_PT_BACKGROUND,
		CSS_PT_BACKGROUND_COLOR,
		CSS_PT_COLOR,
		CSS_PT_DISPLAY,
		CSS_PT_FONT_STYLE,
		CSS_PT_FONT_WEIGHT,
		CSS_PT_TEXT_ALIGN,
		CSS_PT_TEXT_DECORATION,
		CSS_PT_WHITE_SPACE,
		CSS_PT_LAST,
	} type;

	/** Type of the property value.  Discriminates the #value union.  */
	enum css_property_value_type {
		CSS_VT_NONE,
		CSS_VT_COLOR,
		CSS_VT_DISPLAY,
		CSS_VT_FONT_ATTRIBUTE,
		CSS_VT_TEXT_ALIGN,
		CSS_VT_LAST,
	} value_type;

	/** Property value. If it is a pointer, it points always to a
	 * memory to be free()d together with this structure. */
	union css_property_value {
		void *none;
		color_T color;
		enum css_display {
			CSS_DISP_INLINE,
			CSS_DISP_BLOCK,
		} display;
		struct {
			enum text_style_format add, rem;
		} font_attribute;
		enum format_align text_align;
		/* TODO:
		 * Generic numbers
		 * Percentages
		 * URL
		 * Align (struct format_align) */
		/* TODO: The size units will be fun yet. --pasky */
	} value;
};

struct css_property_info;
struct scanner;
typedef int (*css_property_value_parser_T)(struct css_property_info *propinfo,
					 union css_property_value *value,
					 struct scanner *scanner);

/** The struct css_property_info describes what values the properties can
 * have and what internal type they have. */
struct css_property_info {
	unsigned char *name;
	enum css_property_type type;

	/** This is the storage type, basically describing what to save to
	 * css_property.value. Many properties can share the same valtype.
	 * The value is basically output of the value parser. */
	enum css_property_value_type value_type;

	/** This is the property value parser, processing the written
	 * form of a property value. Its job is to take the value
	 * string (or scanner's token list in the future) and
	 * transform it to a union css_property_value according to the
	 * property's #value_type. Although some properties can share
	 * a parser, it is expected that most properties will either
	 * use a custom one or use a generic parser with
	 * property-specific backend specified in #parser_data. */
	css_property_value_parser_T parser;

	/** In case you use a generic #parser, it can be useful to still give
	 * it some specific data. You can do so through @c parser_data. The
	 * content is #parser-specific. */
	void *parser_data;
};

/** This table contains info about all the known CSS properties. */
extern struct css_property_info css_property_info[];

#endif
