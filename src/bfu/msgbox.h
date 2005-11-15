#ifndef EL__BFU_MSGBOX_H
#define EL__BFU_MSGBOX_H

#include "util/memlist.h"

struct terminal;


/* Bitmask specifying some @msg_box() function parameters attributes or
 * altering function operation. See @msg_box() description for details about
 * the flags effect. */
enum msgbox_flags {
	/* {msg_box(.text)} is dynamically allocated */
	MSGBOX_FREE_TEXT = 0x1,
	/* The msg_box() string parameters should NOT be run through gettext
	 * and translated. */
	MSGBOX_NO_INTL = 0x2,
	/* Should the text be scrollable */
	/* XXX: The text need to be allocated since it will be mangled */
	MSGBOX_SCROLLABLE = 0x4,
	/* i18nalize title and buttons but not the text. */
	MSGBOX_NO_TEXT_INTL = 0x8,
};

/* This is _the_ dialog function used in almost all parts of the code. It is
 * used to easily format dialogs containing only text and few buttons below.
 *
 * @term	The terminal where the message box should appear.
 *
 * @mem_list	A list of pointers to allocated memory that should be
 *		free()'d when then dialog is closed. The list can be
 *		initialized using getml(args, NULL) using NULL as the end.
 *		This is useful especially when you pass stuff to @udata
 *		which you want to be free()d when not needed anymore.
 *
 * @flags	If the MSGBOX_FREE_TEXT flag is passed, @text is free()d upon
 *		the dialog's death. This is equivalent to adding @text to the
 *		@mem_list. Also, when this parameter is passed, @text is not
 *		automagically localized and it is up to the user to do it.
 *
 *		If the MSGBOX_NO_INTL flag is passed, @title, @text and button
 *		labels will not be localized automatically inside of msg_box()
 *		and it is up to the user to do the best job possible.
 *
 * @title	The title of the message box. It is automatically localized
 * 		inside (unless MSGBOX_NO_INTL is passed).
 *
 * @align	Provides info about how @text should be aligned.
 *
 * @text	The info text of the message box. If the text requires
 *		formatting use msg_text(format, args...). This will allocate
 *		a string so remember to @align |= MSGBOX_FREE_TEXT.
 *
 *		If no formatting is needed just pass the string and don't
 *		@align |= MSGBOX_FREE_TEXT or you will get in trouble. ;)
 *
 *		The @text is automatically localized inside of msg_box(),
 *		unless MSGBOX_NO_INTL or MSGBOX_FREE_TEXT is passed. That is
 *		because you do NOT want to localize output of msg_text(),
 *		but rather individually the format string and parameters to
 *		its string conversions.
 *
 * @udata	Is a reference to any data that should be passed to
 *		the handlers associated with each button. NULL if none.
 *
 * @buttons	Denotes the number of buttons given as varadic arguments.
 *		For each button 3 arguments are extracted:
 *			o First the label text. It is automatically localized
 *			  unless MSGBOX_NO_INTL is passed. If NULL, this button
 *			  is skipped.
 *			o Second pointer to the handler function (taking
 *			  one (void *), which is incidentally the udata).
 *			o Third any flags.
 *
 * Note that you should ALWAYS format the msg_box() call like:
 *
 * msg_box(term, mem_list, flags,
 *         title, align,
 *         text,
 *         udata, M,
 *         label1, handler1, flags1,
 *         ...,
 *         labelM, handlerM, flagsM);
 *
 * ...no matter that it could fit on one line in case of a tiny message box. */
struct dialog_data *
msg_box(struct terminal *term, struct memory_list *mem_list,
	enum msgbox_flags flags, unsigned char *title, enum format_align align,
	unsigned char *text, void *udata, int buttons, ...);


/* msg_text() is basically an equivalent to asprintf(), specifically to be used
 * inside of message boxes. Please always use msg_text() instead of asprintf()
 * in msg_box() parameters (ie. own format conversions can be introduced later
 * specifically crafted for message boxes, and so on).
 * The returned string is allocated and may be NULL!
 * This one automagically localizes the format string. The possible
 * additional parameters still need to be localized manually at the user's
 * side. */
unsigned char *msg_text(struct terminal *term, unsigned char *format, ...);

/* A periodically refreshed message box with one OK button. The text in the
 * message box is updated using the get_info() function. If get_info() returns
 * NULL the message box is closed. */
void
refreshed_msg_box(struct terminal *term, enum msgbox_flags flags,
		  unsigned char *title, enum format_align align,
		  unsigned char *(get_info)(struct terminal *, void *),
		  void *data);

struct dialog_data *
info_box(struct terminal *term, enum msgbox_flags flags,
	 unsigned char *title, enum format_align align,
	 unsigned char *text);


#endif
