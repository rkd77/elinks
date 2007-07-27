#ifndef EL__TERMINAL_KBD_H
#define EL__TERMINAL_KBD_H

#include "intl/charsets.h"

struct itrm;

/** A character received from a terminal.  */
#ifdef CONFIG_UTF8
typedef unicode_val_T term_event_char_T; /* in UCS-4 */
#else
typedef unsigned char term_event_char_T; /* in the charset of the terminal */
#endif

/** A key received from a terminal, without modifiers.  The value is
 * either from term_event_char_T or from enum term_event_special_key.
 * To check which one it is, use is_kbd_character().
 *
 * - Values <= -0x100 are special; from enum term_event_special_key.
 * - Values between -0xFF and -2 are not used yet; treat as special.
 * - Value  == -1 is KBD_UNDEF; not sent via socket.
 * - Values >= 0 are characters; from term_event_char_T.
 *
 * Any at least 32-bit signed integer type would work here; using an
 * exact-width type hurts portability in principle, but some other
 * parts of ELinks already require the existence of uint32_t.  */
typedef int32_t term_event_key_T;

/** Values for term_event_keyboard.modifier and
 * interlink_event_keyboard.modifier */
typedef enum {
	KBD_MOD_NONE	= 0,
	KBD_MOD_SHIFT	= 1,
	KBD_MOD_CTRL	= 2,
	KBD_MOD_ALT	= 4
} term_event_modifier_T;

/** A key received from a terminal, with modifiers.  */
struct term_event_keyboard {
	term_event_key_T key;
	term_event_modifier_T modifier;
};

/** Like struct term_event_keyboard but used in the interlink protocol
 * between ELinks processes.  Because the processes may be running
 * different versions of ELinks, especially if a new version has just
 * been installed, this structure should be kept binary compatible as
 * long as possible.  See bug 793 for a list of pending changes to the
 * protocol.  */
struct interlink_event_keyboard {
	/** This is like term_event_key_T but carries individual bytes
	 * rather than entire characters, and uses different values
	 * for special keys.
	 *
	 * - Values <= -2 are not used, for ELinks 0.11 compatibility.
	 * - Value  == -1 is KBD_UNDEF; not sent via socket.
	 * - Values between 0 and 0xFF are bytes received from the terminal.
	 * - Values >= 0x100 are special; absolute values of constants
	 *   from enum term_event_special_key, e.g. -KBD_ENTER.  */
	int key;
	/** The values are from term_event_modifier_T, but the type
	 * must be int so that the representation remains compatible
	 * with ELinks 0.11.  */
	int modifier;
};

/** Codes of special keys, for use in term_event_key_T.
 * The enum has a tag mainly to let you cast numbers to it in GDB and see
 * their names.  ELinks doesn't use this enum type as term_event_key_T,
 * because it might be 16-bit and Unicode characters wouldn't then fit.  */
enum term_event_special_key {
	KBD_UNDEF 	= -1,

	KBD_ENTER	= -0x100,
	KBD_BS		= -0x101,
	KBD_TAB		= -0x102,
	KBD_ESC		= -0x103,
	KBD_LEFT	= -0x104,
	KBD_RIGHT	= -0x105,
	KBD_UP		= -0x106,
	KBD_DOWN	= -0x107,
	KBD_INS		= -0x108,
	KBD_DEL		= -0x109,
	KBD_HOME	= -0x10a,
	KBD_END		= -0x10b,
	KBD_PAGE_UP	= -0x10c,
	KBD_PAGE_DOWN	= -0x10d,

	KBD_F1		= -0x120,
	KBD_F2		= -0x121,
	KBD_F3		= -0x122,
	KBD_F4		= -0x123,
	KBD_F5		= -0x124,
	KBD_F6		= -0x125,
	KBD_F7		= -0x126,
	KBD_F8		= -0x127,
	KBD_F9		= -0x128,
	KBD_F10		= -0x129,
	KBD_F11		= -0x12a,
	KBD_F12		= -0x12b,

	KBD_CTRL_C	= -0x200
};

static inline int is_kbd_fkey(term_event_key_T key) { return key <= KBD_F1 && key >= KBD_F12; }
#define number_to_kbd_fkey(num) (KBD_F1 - (num) + 1)
#define kbd_fkey_to_number(key) (KBD_F1 - (key) + 1)

/**Check whether @a key is a character or a special key.
 * @code int is_kbd_character(term_event_key_T key); @endcode
 * Return true if @a key is a character from ::term_event_char_T.
 * (The character is not necessarily printable.)
 * Return false if @a key is a special key from enum term_event_special_key.  */
#define is_kbd_character(key) ((key) >= 0)

void
handle_trm(int std_in, int std_out, int sock_in, int sock_out, int ctl_in,
	   void *init_string, int init_len, int remote);

void itrm_queue_event(struct itrm *itrm, unsigned char *data, int len);
void block_itrm(void);
int unblock_itrm(void);
void free_all_itrms(void);
void resize_terminal(void);
void dispatch_special(unsigned char *);
void kbd_ctrl_c(void);
int is_blocked(void);
void get_terminal_name(unsigned char *);

#define kbd_get_key(kbd_)	((kbd_)->key)
#define kbd_key_is(kbd_, key)	(kbd_get_key(kbd_) == (key))

#define kbd_get_modifier(kbd_)	((kbd_)->modifier)
#define kbd_modifier_is(kbd_, mod)	(kbd_get_modifier(kbd_) == (mod))

#define kbd_set(kbd_, key_, modifier_) do { \
	(kbd_)->key = (key_);	\
	(kbd_)->modifier = (modifier_);	\
} while (0)


#endif
