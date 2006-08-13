#ifndef EL__TERMINAL_KBD_H
#define EL__TERMINAL_KBD_H

struct itrm;

/* Values <= -0x100 are special; e.g. KBD_ENTER.
 * Values between -0xFF and -2 are not used yet; treat as special.
 * Value  == -1 is KBD_UNDEF; not sent via socket. 
 * Values >= 0 are characters received from the terminal;
 * in UCS-4 #ifdef CONFIG_UTF_8.
 *
 * Any at least 32-bit signed integer type would work here; using an
 * exact-width type hurts portability in principle, but some other
 * parts of ELinks already require the existence of uint32_t.  */
typedef int32_t term_event_key_T;

struct term_event_keyboard {
	term_event_key_T key;
	int modifier;
};

struct interlink_event_keyboard {
	/* Values <= -2 are not used, for ELinks 0.11 compatibility.
	 * Value  == -1 is KBD_UNDEF; not sent via socket.
	 * Values between 0 and 0xFF are bytes received from the terminal.
	 * Values >= 0x100 are special; e.g. -KBD_ENTER.  */
	int key;
	int modifier;
};

/* Values for term_event_key_T */

#define KBD_UNDEF	-1

#define KBD_ENTER	(-0x100)
#define KBD_BS		(-0x101)
#define KBD_TAB		(-0x102)
#define KBD_ESC		(-0x103)
#define KBD_LEFT	(-0x104)
#define KBD_RIGHT	(-0x105)
#define KBD_UP		(-0x106)
#define KBD_DOWN	(-0x107)
#define KBD_INS		(-0x108)
#define KBD_DEL		(-0x109)
#define KBD_HOME	(-0x10a)
#define KBD_END		(-0x10b)
#define KBD_PAGE_UP	(-0x10c)
#define KBD_PAGE_DOWN	(-0x10d)

#define KBD_F1		(-0x120)
#define KBD_F2		(-0x121)
#define KBD_F3		(-0x122)
#define KBD_F4		(-0x123)
#define KBD_F5		(-0x124)
#define KBD_F6		(-0x125)
#define KBD_F7		(-0x126)
#define KBD_F8		(-0x127)
#define KBD_F9		(-0x128)
#define KBD_F10		(-0x129)
#define KBD_F11		(-0x12a)
#define KBD_F12		(-0x12b)
static inline int is_kbd_fkey(term_event_key_T key) { return key <= KBD_F1 && key >= KBD_F12; }
#define number_to_kbd_fkey(num) (KBD_F1 - (num) + 1)
#define kbd_fkey_to_number(key) (KBD_F1 - (key) + 1)

#define KBD_CTRL_C	(-0x200)

/* int is_kbd_character(term_event_key_T key);
 * Return true if @key is a character in some charset, rather than a
 * special key.  The character is not necessarily printable.  As for
 * which charset it is in, see the definition of term_event_key_T.  */
#define is_kbd_character(key) ((key) >= 0)

/* Values for term_event_keyboard.modifier and
 * interlink_event_keyboard.modifier */
#define KBD_MOD_NONE	0
#define KBD_MOD_SHIFT	1
#define KBD_MOD_CTRL	2
#define KBD_MOD_ALT	4

void
handle_trm(int std_in, int std_out, int sock_in, int sock_out, int ctl_in,
	   void *init_string, int init_len, int remote);

void itrm_queue_event(struct itrm *itrm, unsigned char *data, int len);
void block_itrm(int);
int unblock_itrm(int);
void free_all_itrms(void);
void resize_terminal(void);
void dispatch_special(unsigned char *);
void kbd_ctrl_c(void);
int is_blocked(void);

#define kbd_get_key(kbd_)	((kbd_)->key)
#define kbd_key_is(kbd_, key)	(kbd_get_key(kbd_) == (key))

#define kbd_get_modifier(kbd_)	((kbd_)->modifier)
#define kbd_modifier_is(kbd_, mod)	(kbd_get_modifier(kbd_) == (mod))

#define kbd_set(kbd_, key_, modifier_) do { \
	(kbd_)->key = (key_);	\
	(kbd_)->modifier = (modifier_);	\
} while (0)


#endif
