/* $Id: kbd.h,v 1.14 2005/09/14 12:32:49 zas Exp $ */

#ifndef EL__TERMINAL_KBD_H
#define EL__TERMINAL_KBD_H

struct itrm;

struct itrm *ditrm;

struct term_event_keyboard {
	int key;
	int modifier;
};

#define KBD_UNDEF	-1

#define KBD_ENTER	0x100
#define KBD_BS		0x101
#define KBD_TAB		0x102
#define KBD_ESC		0x103
#define KBD_LEFT	0x104
#define KBD_RIGHT	0x105
#define KBD_UP		0x106
#define KBD_DOWN	0x107
#define KBD_INS		0x108
#define KBD_DEL		0x109
#define KBD_HOME	0x10a
#define KBD_END		0x10b
#define KBD_PAGE_UP	0x10c
#define KBD_PAGE_DOWN	0x10d

#define KBD_F1		0x120
#define KBD_F2		0x121
#define KBD_F3		0x122
#define KBD_F4		0x123
#define KBD_F5		0x124
#define KBD_F6		0x125
#define KBD_F7		0x126
#define KBD_F8		0x127
#define KBD_F9		0x128
#define KBD_F10		0x129
#define KBD_F11		0x12a
#define KBD_F12		0x12b

#define KBD_CTRL_C	0x200

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
