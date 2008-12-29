#ifndef EL__TERMINAL_TERMINAL_H
#define EL__TERMINAL_TERMINAL_H

#include "config/options.h"
#include "terminal/event.h"
#include "util/lists.h"

struct module;
struct option;
struct terminal_screen;
struct terminal_interlink;


/** The terminal type, meaningful for frames (lines) drawing. */
enum term_mode_type {
	TERM_DUMB = 0,
	TERM_VT100,
	TERM_LINUX,
	TERM_KOI8,
	TERM_FREEBSD,
};

/** This is a bitmask describing the environment we are living in,
 * terminal-wise. We can then conditionally use various features available
 * in such an environment. */
enum term_env_type {
	/** This basically means that we can use the text i/o :). Always set. */
	ENV_CONSOLE = 1,
	/** We are running in a xterm-compatible box in some windowing
	 * environment. */
	ENV_XWIN = 2,
	/** We are running under a screen. */
	ENV_SCREEN = 4,
	/** We are running in a OS/2 VIO terminal. */
	ENV_OS2VIO = 8,
	/** BeOS text terminal. */
	ENV_BE = 16,
	/** We live in a TWIN text-mode windowing environment. */
	ENV_TWIN = 32,
	/** Microsoft Windows cmdline thing. */
	ENV_WIN32 = 64,
	/** Match all terminal environments */
	ENV_ANY = ~0,
};

enum term_redrawing_state {
	TREDRAW_READY = 0,	/**< Can redraw */
	TREDRAW_BUSY = 1,	/**< Redrawing already in progress */
	TREDRAW_DELAYED = 2,	/**< Do not redraw for now */
};

/** This is one of the axis of ELinks' user interaction. struct terminal
 * defines the terminal ELinks is running on --- each ELinks instance has
 * one. It contains the basic terminal attributes, the settings associated
 * with this terminal, screen content (and more abstract description of what
 * is currently displayed on it) etc. It also maintains some runtime
 * information about the actual ELinks instance owning this terminal.
 *
 * @todo TODO: Regroup the following into logical chunks. --pasky */
struct terminal {
	LIST_HEAD(struct terminal); /*!< ::terminals is the sentinel.  */

	/** This is (at least partially) a stack of all the windows living in
	 * this terminal. A window can be wide range of stuff, from a menu box
	 * through classical dialog window to a tab. See terminal/window.h for
	 * more on windows.
	 *
	 * Tabs are special windows, though, and you never want to display them
	 * all, but only one of them. ALWAYS check inactive_tab() during
	 * iterations through this list (unless it is really useless or you
	 * are sure what are you doing) to make sure that you don't distribute
	 * events etc to inactive tabs.
	 *
	 * The stack is top-down, thus @c .next is the stack's top, the
	 * current window; and @c .prev is the bottom, covered by others.
	 * - Dialogs or active menus are at the top.
	 * - Next come all tabs (window.type == ::WINDOW_TAB).  The tab
	 *   listed leftmost in the tab bar is at the top, and the tab
	 *   listed rightmost is at the bottom; but #current_tab controls
	 *   which tab is actually displayed.
	 * - If the main menu is inactive, then it is at the very bottom,
	 *   hidden under the tabs.
	 * Call assert_window_stacking() to verify this.
	 *
	 * @todo FIXME: Tabs violate the stack nature of this list, they
	 * appear there randomly but always in the order in which they were
	 * inserted there.  Eventually, they should all live at the stack
	 * bottom, with the actual tab living on the VERY bottom. --pasky
	 *
	 *   Keeping the current tab at the very bottom would require storing
	 *   tab numbers explicitly, rather than computing them from the
	 *   stack order as is done now.  Also, what should be done with the
	 *   inactive main menu?  --KON */
	LIST_OF(struct window) windows;

	/** The specification of terminal in terms of terminal options. */
	struct option *spec;

	/** This is the terminal's current title, as perhaps displayed
	 * somewhere in the X window frame or so. */
	unsigned char *title;

	/** This is the screen. See terminal/screen.h */
	struct terminal_screen *screen;

	/** This is for displaying main menu */
	struct menu *main_menu;

	/** These are pipes for communication with the ELinks instance owning
	 * this terminal.
	 * @see struct itrm */
	int fdin, fdout;

	/** This indicates that the terminal is blocked, that is nothing should
	 * be drawn on it etc. Typically an external program is running on it
	 * right now. This is a file descriptor. */
	int blocked;

	/** Terminal dimensions. */
	int width, height;

	/** Indicates whether we are currently in the process of redrawing the
	 * stuff being displayed on the terminal. It is typically used to
	 * prevent redrawing inside of redrawing. */
	enum term_redrawing_state redrawing;

	/** Indicates the master terminal, that is the terminal under
	 * supervision of the master ELinks instance (the one doing all the
	 * work and even maintaining these structures ;-). */
	unsigned int master:1;

#ifdef CONFIG_UTF8
	/** Indicates whether the charset of the terminal is UTF-8.  */
	unsigned int utf8_cp:1;

	/** Indicates whether UTF-8 I/O is used.  Forced on if the
	 * UTF-8 charset is selected.  (bug 827) */
	unsigned int utf8_io:1;
#endif /* CONFIG_UTF8 */

	/** The current tab number. */
	int current_tab;

#ifdef CONFIG_LEDS
	/** Current length of leds part of status bar. */
	int leds_length;
#endif

	/** The type of environment this terminal lives in. */
	enum term_env_type environment;

	/** The current working directory for this terminal / ELinks instance. */
	unsigned char cwd[MAX_CWD_LEN];

	/** For communication between instances */
	struct terminal_interlink *interlink;

	struct term_event_mouse prev_mouse_event;
};

#define do_not_ignore_next_mouse_event(term) \
	memset(&(term)->prev_mouse_event, 0, sizeof((term)->prev_mouse_event))

/** We keep track about all the terminals in this list. */
extern LIST_OF(struct terminal) terminals;


extern const unsigned char frame_dumb[];

struct terminal *init_term(int, int);
void destroy_terminal(struct terminal *);
void redraw_terminal(struct terminal *term);
void redraw_terminal_cls(struct terminal *term);
void cls_redraw_all_terminals(void);
int get_terminal_codepage(const struct terminal *);

void redraw_all_terminals(void);
void destroy_all_terminals(void);
void exec_thread(unsigned char *, int);
void close_handle(void *);

#ifdef CONFIG_FASTMEM
#define assert_terminal_ptr_not_dangling(suspect) ((void) 0)
#else  /* assert() does something */
void assert_terminal_ptr_not_dangling(const struct terminal *);
#endif

/** Operations that can be requested with do_terminal_function() in
 * the master and then executed with dispatch_special() in a slave.
 * The interlink protocol passes these values as one byte in a
 * null-terminated string, so zero cannot be used.  */
enum {
	TERM_FN_TITLE	       = 1,
	TERM_FN_RESIZE         = 2,
	TERM_FN_TITLE_CODEPAGE = 3
};

/** How to execute a program in a terminal.  These values are used in
 * the interlink protocol and must fit in one byte.  */
enum term_exec {
	/** Execute in the background.  ELinks keeps using the terminal
	 * and the program should not use it.  */
	TERM_EXEC_BG = 0,

	/** Execute in the foreground.  The program may use the terminal.
	 * ELinks will redraw when the program exits.  */
	TERM_EXEC_FG = 1,

	/** Execute in the background and in a new process group.  */
	TERM_EXEC_NEWWIN = 2
};

void exec_on_terminal(struct terminal *, unsigned char *, unsigned char *, enum term_exec);
void exec_shell(struct terminal *term);

int set_terminal_title(struct terminal *, unsigned char *);
void do_terminal_function(struct terminal *, unsigned char, unsigned char *);

int check_terminal_pipes(void);
void close_terminal_pipes(void);
struct terminal *attach_terminal(int in, int out, int ctl, void *info, int len);

extern struct module terminal_module;

#endif /* EL__TERMINAL_TERMINAL_H */
