#ifndef EL__TERMINAL_EVENT_H
#define EL__TERMINAL_EVENT_H

#include "terminal/kbd.h"
#include "terminal/mouse.h"

struct terminal;

/* Some constants for the strings inside of {struct terminal}. */

#define MAX_TERM_LEN	32	/* this must be multiple of 8! (alignment problems) */
#define MAX_CWD_LEN	256	/* this must be multiple of 8! (alignment problems) */


/** Type of an event received from a terminal.  */
enum term_event_type {
	EVENT_INIT,
	EVENT_KBD,
	EVENT_MOUSE,
	EVENT_REDRAW,
	EVENT_RESIZE,
	EVENT_ABORT,
};

/** An event received from a terminal.  This type can be changed
 * without breaking interlink compatibility.  */
struct term_event {
	enum term_event_type ev;

	union {
		/** ::EVENT_MOUSE */
		struct term_event_mouse mouse;

		/** ::EVENT_KBD */
		struct term_event_keyboard keyboard;

		/** ::EVENT_INIT, ::EVENT_RESIZE, ::EVENT_REDRAW */
		struct term_event_size {
			int width, height;
		} size;
	} info;
};

/** An event transferred via the interlink socket.  This is quite
 * similar to struct term_event but has a different format for
 * keyboard events.  If you change this type, you can break interlink
 * compatibility.  */
struct interlink_event {
	enum term_event_type ev;

	union {
		/* ::EVENT_MOUSE */
		struct interlink_event_mouse mouse;

		/* ::EVENT_KBD */
		struct interlink_event_keyboard keyboard;

		/* ::EVENT_INIT, ::EVENT_RESIZE, ::EVENT_REDRAW */
#define interlink_event_size term_event_size
		struct interlink_event_size size;
	} info;
};

static inline void
set_mouse_term_event(struct term_event *ev, int x, int y, unsigned int button)
{
	memset(ev, 0, sizeof(*ev));
	ev->ev = EVENT_MOUSE;
	set_mouse(&ev->info.mouse, x, y, button);
}

static inline void
set_mouse_interlink_event(struct interlink_event *ev, int x, int y, unsigned int button)
{
	memset(ev, 0, sizeof(*ev));
	ev->ev = EVENT_MOUSE;
	set_mouse(&ev->info.mouse, x, y, button);
}

static inline void
set_kbd_term_event(struct term_event *ev, int key,
		   term_event_modifier_T modifier)
{
	memset(ev, 0, sizeof(*ev));
	ev->ev = EVENT_KBD;
	kbd_set(&ev->info.keyboard, key, modifier);
}

/** Initialize @c ev as an interlink keyboard event.
 * @a key can be either an 8-bit byte or a value from enum
 * term_event_special_key.  In the latter case, this function negates
 * the value, unless it is KBD_UNDEF.  For example, key == KBD_ENTER
 * results in ev->info.keyboard.key = -KBD_ENTER.  This mapping keeps
 * the interlink protocol compatible with ELinks 0.11.  */
static inline void
set_kbd_interlink_event(struct interlink_event *ev, int key,
			term_event_modifier_T modifier)
{
	memset(ev, 0, sizeof(*ev));
	ev->ev = EVENT_KBD;
	if (key <= -0x100)
		key = -key;
	kbd_set(&ev->info.keyboard, key, modifier);
}

static inline void
set_abort_term_event(struct term_event *ev)
{
	memset(ev, 0, sizeof(*ev));
	ev->ev = EVENT_ABORT;
}

static inline void
set_wh_term_event(struct term_event *ev, enum term_event_type type, int width, int height)
{
	memset(ev, 0, sizeof(*ev));
	ev->ev = type;
	ev->info.size.width = width;
	ev->info.size.height = height;
}

#define set_init_term_event(ev, w, h) set_wh_term_event(ev, EVENT_INIT, w, h)
#define set_resize_term_event(ev, w, h) set_wh_term_event(ev, EVENT_RESIZE, w, h)
#define set_redraw_term_event(ev, w, h) set_wh_term_event(ev, EVENT_REDRAW, w, h)

static inline void
set_wh_interlink_event(struct interlink_event *ev, enum term_event_type type, int width, int height)
{
	memset(ev, 0, sizeof(*ev));
	ev->ev = type;
	ev->info.size.width = width;
	ev->info.size.height = height;
}

#define set_resize_interlink_event(ev, w, h) set_wh_interlink_event(ev, EVENT_RESIZE, w, h)


/** This holds the information used when handling the initial
 * connection between a dumb and master terminal.
 *
 * XXX: We might be connecting to an older ELinks or an older ELinks is
 * connecting to a newer ELinks master so for the sake of compatibility it
 * would be unwise to just change the layout of the struct. If you do have to
 * add new members add them at the bottom and use magic variables to
 * distinguish them when decoding the terminal info. */
struct terminal_info {
	struct interlink_event event;		/**< The ::EVENT_INIT event */
	unsigned char name[MAX_TERM_LEN];	/**< $TERM environment name */
	unsigned char cwd[MAX_CWD_LEN];		/**< Current working directory */
	int system_env;				/**< System info (X, screen) */
	int length;				/**< Length of #data member */
	int session_info;			/**< Value depends on #magic */
	int magic;				/**< Identity of the connector */

	/** In the master that is connected to all bytes after @c data
	 * will be interpreted as URI string information. */
	unsigned char data[1];
};

/** The terminal_info.data member has to have size of one for
 * portability but it can be empty/zero so when reading and writing it
 * we need to ignore the byte. */
#define TERMINAL_INFO_SIZE offsetof(struct terminal_info, data)

/** We use magic numbers to signal the identity of the dump client terminal.
 * Magic numbers are composed by the INTERLINK_MAGIC() macro. It is a negative
 * magic to be able to distinguish the oldest format from the newer ones. */
#define INTERLINK_MAGIC(major, minor) -(((major) << 8) + (minor))

#define INTERLINK_NORMAL_MAGIC INTERLINK_MAGIC(1, 0)
#define INTERLINK_REMOTE_MAGIC INTERLINK_MAGIC(1, 1)

void term_send_event(struct terminal *, struct term_event *);
void in_term(struct terminal *);

/** @name For keyboard events handling
 * @{ */
#define get_kbd_key(event)		(kbd_get_key(&(event)->info.keyboard))
#define check_kbd_key(event, key)	(kbd_key_is(&(event)->info.keyboard, (key)))

#define get_kbd_modifier(event)		(kbd_get_modifier(&(event)->info.keyboard))
#define check_kbd_modifier(event, mod)	(kbd_modifier_is(&(event)->info.keyboard, (mod)))

#define check_kbd_textinput_key(event)	(get_kbd_key(event) >= ' ' && check_kbd_modifier(event, KBD_MOD_NONE))
#define check_kbd_label_key(event)	(get_kbd_key(event) > ' ' && (check_kbd_modifier(event, KBD_MOD_NONE) || check_kbd_modifier(event, KBD_MOD_ALT)))
/** @} */


/** @name For mouse events handling
 * @{ */
#define get_mouse_action(event)		 (mouse_get_action(&(event)->info.mouse))
#define check_mouse_action(event, value) (mouse_action_is(&(event)->info.mouse, (value)))

#define get_mouse_button(event)		 (mouse_get_button(&(event)->info.mouse))
#define check_mouse_button(event, value) (mouse_button_is(&(event)->info.mouse, (value)))
#define check_mouse_wheel(event)	 (mouse_wheeling(&(event)->info.mouse))

#define check_mouse_position(event, box) \
	mouse_is_in_box(&(event)->info.mouse, box)
/** @} */


#endif /* EL__TERMINAL_EVENT_H */
