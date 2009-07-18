#ifndef EL__BFU_BUTTON_H
#define EL__BFU_BUTTON_H

#include "bfu/common.h"
#include "util/align.h"

struct dialog;
struct terminal;
struct widget_data;

typedef void (done_handler_T)(void *);

struct widget_info_button {
	int flags;
	int hotkey_pos;	/* -1 means no hotkey, hotkeys are marked by ~. */
	int textlen;	/* Text length without hotkey */
	int truetextlen; /* Original text length (with hotkey if any) */
	/* Used by some default handlers like ok_dialog()
	 * as a callback. */
	done_handler_T *done;
	void *done_data;
};

/* Button flags, go into widget.gid */
#define B_ENTER		1
#define B_ESC		2

/* Define to find buttons without keyboard accelerator. */
/* #define DEBUG_BUTTON_HOTKEY */

/** @def add_dlg_ok_button
 * Add a button that will close the dialog if pressed.
 *
 * void add_dlg_ok_button(struct dialog *dlg, unsigned char *text, int flags,
 *                        ::done_handler_T *done, void *data);
 *
 * @param dlg
 * The dialog in which the button is to be added.
 *
 * @param text
 * Text displayed in the button.  This string should contain a
 * keyboard accelerator, marked with a preceding '~'.  The pointer
 * must remain valid as long as the dialog exists.
 *
 * @param flags
 * Can be ::B_ENTER, ::B_ESC, or 0.
 *
 * @param done
 * A function that BFU calls when the user presses this button.
 * Before calling this, BFU checks the values of widgets.
 * After the function returns, BFU closes the dialog.
 *
 * @param data
 * A pointer to be passed to the @a done callback.  */

/** @def add_dlg_button
 * Add a button that need not close the dialog if pressed.
 *
 * void add_dlg_button(struct dialog *dlg, unsigned char *text, int flags,
 *                     ::widget_handler_T *handler, void *data);
 *
 * @param handler
 * A function that BFU calls when the user presses this button.
 * BFU does not automatically check the values of widgets
 * or close the dialog.
 *
 * @param data
 * A pointer to any data needed by @a handler.  It does not get this
 * pointer as a parameter but can read it from widget_data->widget->data.
 *
 * The other parameters are as in ::add_dlg_ok_button.  */

#ifdef DEBUG_BUTTON_HOTKEY
void add_dlg_button_do(const unsigned char *file, int line, struct dialog *dlg, unsigned char *text, int flags, widget_handler_T *handler, void *data, done_handler_T *done, void *done_data);
#define add_dlg_ok_button(dlg, text, flags, done, data)	\
	add_dlg_button_do(__FILE__, __LINE__, dlg, text, flags, ok_dialog, NULL, done, data)

#define add_dlg_button(dlg, text, flags, handler, data)	\
	add_dlg_button_do(__FILE__, __LINE__, dlg, text, flags, handler, data, NULL, NULL)

#else
void add_dlg_button_do(struct dialog *dlg, unsigned char *text, int flags, widget_handler_T *handler, void *data, done_handler_T *done, void *done_data);

#define add_dlg_ok_button(dlg, text, flags, done, data)	\
	add_dlg_button_do(dlg, text, flags, ok_dialog, NULL, done, data)

#define add_dlg_button(dlg, text, flags, handler, data)	\
	add_dlg_button_do(dlg, text, flags, handler, data, NULL, NULL)
#endif

extern const struct widget_ops button_ops;
void dlg_format_buttons(struct terminal *, struct widget_data *, int, int, int *, int, int *, enum format_align, int);

#endif
