#ifndef EL__BFU_COMMON_H
#define EL__BFU_COMMON_H

struct dialog_data;
struct widget_data;

/* Event handlers return this values */
typedef enum {
	EVENT_PROCESSED	= 0,
	EVENT_NOT_PROCESSED = 1
} widget_handler_status_T;

/* Handler type for widgets. */
typedef widget_handler_status_T (widget_handler_T)(struct dialog_data *, struct widget_data *);

/** Type of widget.
 *
 * Each type has a struct widget_ops that contains pointers to
 * type-specific handler functions.  For example, field_ops.kbd points
 * to a function that handles keyboard events for WIDGET_FIELD.  */
enum widget_type {
	/** A check box or a radio button.
	 * @see checkbox_ops */
	WIDGET_CHECKBOX,

	/** A single-line input field.
	 * @see field_ops */
	WIDGET_FIELD,

	/** A single-line input field for a password.
	 * Like #WIDGET_FIELD but shows asterisks instead of the text.
	 * @see field_pass_ops */
	WIDGET_FIELD_PASS,

	/** A button that the user can push.
	 * @see button_ops */
	WIDGET_BUTTON,

	/** A list or tree of items.
	 * @see listbox_ops */
	WIDGET_LISTBOX,

	/** A block of text that the user cannot edit but may be able
	 * to scroll.
	 * @see text_ops */
	WIDGET_TEXT,
};


#endif /* EL__BFU_COMMON_H */
