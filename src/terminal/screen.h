#ifndef EL__TERMINAL_SCREEN_H
#define EL__TERMINAL_SCREEN_H

#ifdef __cplusplus
extern "C" {
#endif

struct bitfield;
struct module;
struct screen_char;
struct string;
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

	/** Whether to redraw screen */
	unsigned int was_dirty:1;

	struct bitfield *dirty;
	struct bitfield *dirty_image;
};

/** Mark the screen ready for redrawing. */
void set_screen_dirty(struct terminal_screen *screen, int from, int to);
void set_screen_dirty_image(struct terminal_screen *screen, int from, int to);

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

struct string *add_cursor_move_to_string(struct string *screen, int y, int x);

extern struct module terminal_screen_module;

#ifdef __cplusplus
}
#endif

#endif
