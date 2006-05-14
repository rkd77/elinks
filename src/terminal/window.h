#ifndef EL__TERMINAL_WINDOW_H
#define EL__TERMINAL_WINDOW_H

#include "util/lists.h"

struct term_event;
struct terminal;
struct window;

enum window_type {
	/* Normal windows: */
	/* Used for things like dialogs. The default type when adding windows
	 * with add_window(). */
	WINDOW_NORMAL,

	/* Tab windows: */
	/* Tabs are a separate session and has separate history, current
	 * document and action-in-progress .. basically a separate browsing
	 * state. */
	WINDOW_TAB,
};

typedef void (window_handler_T)(struct window *, struct term_event *);

struct window {
	LIST_HEAD(struct window);

	enum window_type type;

	/* The window event handler */
	window_handler_T *handler;

	/* For tab windows the session is stored in @data. For normal windows
	 * it can contain dialog data. */
	/* It is free()'d by delete_window() */
	void *data;

	/* The terminal (and screen) that hosts the window */
	struct terminal *term;

	/* Used for tabs focus detection. */
	int xpos, width;
	int x, y;

	/* For delayed tab resizing */
	unsigned int resize:1;
};

void redraw_from_window(struct window *);
void redraw_below_window(struct window *);
void add_window(struct terminal *, window_handler_T, void *);
void delete_window(struct window *);
void delete_window_ev(struct window *, struct term_event *ev);
#define set_window_ptr(window, x_, y_) do { (window)->x = (x_); (window)->y = (y_); } while (0)
void get_parent_ptr(struct window *, int *, int *);

void add_empty_window(struct terminal *, void (*)(void *), void *);

#if CONFIG_DEBUG
void assert_window_stacking(struct terminal *);
#else
#define assert_window_stacking(t) ((void) (t))
#endif

#endif
