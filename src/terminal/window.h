#ifndef EL__TERMINAL_WINDOW_H
#define EL__TERMINAL_WINDOW_H

#include "util/lists.h"

struct dialog_data;
struct term_event;
struct terminal;
struct window;

enum window_type {
	/** Normal windows.
	 * Used for things like dialogs. The default type when adding windows
	 * with add_window(). */
	WINDOW_NORMAL,

	/** Tab windows.
	 * Tabs are a separate session and has separate history, current
	 * document and action-in-progress .. basically a separate browsing
	 * state. */
	WINDOW_TAB,
};

typedef void (window_handler_T)(struct window *, struct term_event *);

/** A window in the terminal screen.  This structure does not know the
 * position and size of the window, and no functions are provided for
 * drawing into a window.  Instead, when window.handler draws the
 * window, it should decide the position and size of the window, and
 * then draw directly to the terminal, taking care not to draw outside
 * the window.  Windows generally do not have their own coordinate
 * systems; they get mouse events in the coordinate system of the
 * terminal.  */
struct window {
	LIST_HEAD(struct window); /*!< terminal.windows is the sentinel.  */

	/** Whether this is a normal window or a tab window.  */
	enum window_type type;

	/** The window event handler */
	window_handler_T *handler;

	/** For tab windows the session is stored in @c data.
	 * For normal windows it can contain dialog data.
	 * It is free()'d by delete_window() */
	void *data;

	/** The terminal (and screen) that hosts the window */
	struct terminal *term;

	/** For ::WINDOW_TAB, the position and size in the tab bar.
	 * Updated while the tab bar is being drawn, and read if the
	 * user clicks there with the mouse.  */
	int xpos, width;

	/** The position of something that has focus in the window.
	 * Any popup menus are drawn near this position.
	 * In tab windows, during ::NAVIGATE_CURSOR_ROUTING, this is
	 * also the position of the cursor that the user can move;
	 * there is no separate cursor position for each frame.
	 * In dialog boxes, this is typically the top left corner of
	 * the focused widget, while the cursor is somewhere within
	 * the widget.
	 * @see set_window_ptr, get_parent_ptr, set_cursor */
	int x, y;

	/** For delayed tab resizing */
	unsigned int resize:1;
};

/** Which windows redraw_windows() should redraw.  */
enum windows_to_redraw {
	/** Redraw the windows in front of the specified window,
	 * but not the specified window itself.  */
	REDRAW_IN_FRONT_OF_WINDOW,

	/** Redraw the specified window, and the windows in front of
	 * it.  */
	REDRAW_WINDOW_AND_FRONT,

	/** Redraw the windows behind the specified window,
	 * but not the specified window itself.
	 * Do that even if terminal.redrawing is TREDRAW_BUSY.  */
	REDRAW_BEHIND_WINDOW,
};

void redraw_windows(enum windows_to_redraw, struct window *);
void add_window(struct terminal *, window_handler_T, void *);
void delete_window(struct window *);
void delete_window_ev(struct window *, struct term_event *ev);
#define set_window_ptr(window, x_, y_) do { (window)->x = (x_); (window)->y = (y_); } while (0)
void set_dlg_window_ptr(struct dialog_data *dlg_data, struct window *window, int x, int y);
void get_parent_ptr(struct window *, int *, int *);

void add_empty_window(struct terminal *, void (*)(void *), void *);

#if CONFIG_DEBUG
void assert_window_stacking(struct terminal *);
#else
#define assert_window_stacking(t) ((void) (t))
#endif

#if CONFIG_SCRIPTING_SPIDERMONKEY
int would_window_receive_keypresses(const struct window *);
#endif

#endif
