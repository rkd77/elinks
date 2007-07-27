#ifndef EL__TERMINAL_SCREEN_H
#define EL__TERMINAL_SCREEN_H


struct module;
struct screen_char;
struct terminal;

/** The terminal's screen manages */
struct terminal_screen {
	/** This is the screen's image, character by character. */
	struct screen_char *image;

	/** The previous screen's image, used for optimizing actual drawing. */
	struct screen_char *last_image;

	/** The current and the previous cursor positions. */
	int cx, cy;
	int lcx, lcy;

	/** The range of line numbers that are out of sync with the physical
	 * screen. #dirty_from > #dirty_to means not dirty. */
	int dirty_from, dirty_to;
};

/** Mark the screen ready for redrawing. */
static inline void
set_screen_dirty(struct terminal_screen *screen, int from, int to)
{
	int_upper_bound(&screen->dirty_from, from);
	int_lower_bound(&screen->dirty_to, to);
}

/** Initializes a screen. Returns NULL upon allocation failure. */
struct terminal_screen *init_screen(void);

/** Cleans up after the screen. */
void done_screen(struct terminal_screen *screen);

/** Update the size of the previous and the current screen image to
 * hold @a x time @a y chars. */
void resize_screen(struct terminal *term, int x, int y);

/** Updates the terminal screen. */
void redraw_screen(struct terminal *term);

/** Erases the entire screen and moves the cursor to the upper left corner. */
void erase_screen(struct terminal *term);

/** Meeep! */
void beep_terminal(struct terminal *term);

extern struct module terminal_screen_module;

#endif
