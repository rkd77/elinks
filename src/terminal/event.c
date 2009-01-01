/** Event system support routines.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "main/main.h"			/* terminate */
#include "main/object.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "terminal/event.h"
#include "terminal/kbd.h"
#include "terminal/mouse.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/screen.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"
#include "viewer/timer.h"


/** Information used for communication between ELinks instances */
struct terminal_interlink {
	/** How big the input queue is */
	int qlen;
	/** How much is free */
	int qfreespace;

	/** UTF-8 input key value decoding data. */
	struct {
		unicode_val_T ucs;
		int len;
		unicode_val_T min;
		/** Modifier keys from the key event that carried the
		 * first byte of the character.  We need this because
		 * ELinks sees e.g. ESC U+00F6 as 0x1B 0xC3 0xB6 and
		 * converts it to Alt-0xC3 0xB6, attaching the
		 * modifier to the first byte only.  */
		term_event_modifier_T modifier;
	} utf8;

	/** This is the queue of events as coming from the other
	 * ELinks instance owning the hosting terminal. */
	unsigned char input_queue[1];
};


void
term_send_event(struct terminal *term, struct term_event *ev)
{
	struct window *win;

	assert(ev && term);
	if_assert_failed return;

	switch (ev->ev) {
	case EVENT_INIT:
	case EVENT_RESIZE:
	{
		int width = ev->info.size.width;
		int height = ev->info.size.height;

		if (width < 0 || height < 0) {
			ERROR(gettext("Bad terminal size: %d, %d"),
			      width, height);
			break;
		}

		resize_screen(term, width, height);
		erase_screen(term);
		/* Fall through */
	}
	case EVENT_REDRAW:
		/* Nasty hack to avoid assertion failures when doing -remote
		 * stuff and the client exits right away */
		if (!term->screen->image) break;

		clear_terminal(term);
		term->redrawing = TREDRAW_DELAYED;
		/* Note that you do NOT want to ever go and create new
		 * window inside EVENT_INIT handler (it'll get second
		 * EVENT_INIT here). Perhaps the best thing you could do
		 * is registering a bottom-half handler which will open
		 * additional windows.
		 * --pasky */
		if (ev->ev == EVENT_RESIZE) {
			/* We want to propagate EVENT_RESIZE even to inactive
			 * tabs! Nothing wrong will get drawn (in the final
			 * result) as the active tab is always the first one,
			 * thus will be drawn last here. Thanks, Witek!
			 * --pasky */
			foreachback (win, term->windows)
				win->handler(win, ev);

		} else {
			foreachback (win, term->windows)
				if (!inactive_tab(win))
					win->handler(win, ev);
		}
		term->redrawing = TREDRAW_READY;
		break;

	case EVENT_MOUSE:
	case EVENT_KBD:
	case EVENT_ABORT:
		assert(!list_empty(term->windows));
		if_assert_failed break;

		/* We need to send event to correct tab, not to the first one. --karpov */
		/* ...if we want to send it to a tab at all. --pasky */
		win = term->windows.next;
		if (win->type == WINDOW_TAB) {
			win = get_current_tab(term);
			assertm(win != NULL, "No tab to send the event to!");
			if_assert_failed return;
		}

		win->handler(win, ev);
	}
}

static void
term_send_ucs(struct terminal *term, unicode_val_T u,
	      term_event_modifier_T modifier)
{
#ifdef CONFIG_UTF8
	struct term_event ev;

	set_kbd_term_event(&ev, u, modifier);
	term_send_event(term, &ev);
#else  /* !CONFIG_UTF8 */
	struct term_event ev;
	const unsigned char *recoded;

	set_kbd_term_event(&ev, KBD_UNDEF, modifier);
	recoded = u2cp_no_nbsp(u, get_terminal_codepage(term));
	if (!recoded) recoded = "*";
	while (*recoded) {
		ev.info.keyboard.key = *recoded;
		term_send_event(term, &ev);
		recoded++;
	}
#endif /* !CONFIG_UTF8 */
}

static void
check_terminal_name(struct terminal *term, struct terminal_info *info)
{
	unsigned char name[MAX_TERM_LEN + 10];
	int i;

	/* We check TERM env. var for sanity, and fallback to _template_ if
	 * needed. This way we prevent elinks.conf potential corruption. */
	for (i = 0; info->name[i]; i++) {
		if (isident(info->name[i])) continue;

		usrerror(_("Warning: terminal name contains illicit chars.", term));
		return;
	}

	snprintf(name, sizeof(name), "terminal.%s", info->name);

	/* Unlock the default _template_ option tree that was asigned by
	 * init_term() and get the correct one. */
	object_unlock(term->spec);
	term->spec = get_opt_rec(config_options, name);
	object_lock(term->spec);
#ifdef CONFIG_UTF8
	/* Probably not best place for set this. But now we finally have
	 * term->spec and term->utf8 should be set before decode session info.
	 * --Scrool */
	term->utf8_cp = is_cp_utf8(get_terminal_codepage(term));
	/* Force UTF-8 I/O if the UTF-8 charset is selected.  Various
	 * places assume that the terminal's charset is unibyte if
	 * UTF-8 I/O is disabled.  (bug 827) */
	term->utf8_io = term->utf8_cp
		|| get_opt_bool_tree(term->spec, "utf_8_io");
#endif /* CONFIG_UTF8 */
}

#ifdef CONFIG_MOUSE
static int
ignore_mouse_event(struct terminal *term, struct term_event *ev)
{
	struct term_event_mouse *prev = &term->prev_mouse_event;
	struct term_event_mouse *current = &ev->info.mouse;

	if (check_mouse_action(ev, B_UP)
	    && current->y == prev->y
	    && (current->button & ~BM_ACT) == (prev->button & ~BM_ACT)) {
		do_not_ignore_next_mouse_event(term);

		return 1;
	}

	copy_struct(prev, current);

	return 0;
}
#endif

static int
handle_interlink_event(struct terminal *term, struct interlink_event *ilev)
{
	struct terminal_info *info = NULL;
	struct terminal_interlink *interlink = term->interlink;
	struct term_event tev;

	switch (ilev->ev) {
	case EVENT_INIT:
		if (interlink->qlen < TERMINAL_INFO_SIZE)
			return 0;

		info = (struct terminal_info *) ilev;

		if (interlink->qlen < TERMINAL_INFO_SIZE + info->length)
			return 0;

		info->name[MAX_TERM_LEN - 1] = 0;
		check_terminal_name(term, info);

		memcpy(term->cwd, info->cwd, MAX_CWD_LEN);
		term->cwd[MAX_CWD_LEN - 1] = 0;

		term->environment = info->system_env;

		/* We need to make sure that it is possible to draw on the
		 * terminal screen before decoding the session info so that
		 * handling of bad URL syntax by openning msg_box() will be
		 * possible. */
		set_init_term_event(&tev,
				    ilev->info.size.width,
				    ilev->info.size.height);
		term_send_event(term, &tev);

		/* Either the initialization of the first session failed or we
		 * are doing a remote session so quit.*/
		if (!decode_session_info(term, info)) {
			destroy_terminal(term);
			/* Make sure the user is notified if the initialization
			 * of the first session fails. */
			if (program.terminate) {
				usrerror(_("Failed to create session.", term));
				program.retval = RET_FATAL;
			}
			return 0;
		}

		ilev->ev = EVENT_REDRAW;
		/* Fall through */
	case EVENT_REDRAW:
	case EVENT_RESIZE:
		set_wh_term_event(&tev, ilev->ev,
				  ilev->info.size.width,
				  ilev->info.size.height);
		term_send_event(term, &tev);
		break;

	case EVENT_MOUSE:
#ifdef CONFIG_MOUSE
		reset_timer();
		tev.ev = ilev->ev;
		copy_struct(&tev.info.mouse, &ilev->info.mouse);
		if (!ignore_mouse_event(term, &tev))
			term_send_event(term, &tev);
#endif
		break;

	case EVENT_KBD:
	{
		int utf8_io = -1;
		int key = ilev->info.keyboard.key;
		term_event_modifier_T modifier = ilev->info.keyboard.modifier;

		if (key >= 0x100)
			key = -key;

		reset_timer();

		if (modifier == KBD_MOD_CTRL && (key == 'l' || key == 'L')) {
			redraw_terminal_cls(term);
			break;

		} else if (key == KBD_CTRL_C) {
			destroy_terminal(term);
			return 0;
		}

		/* Character Conversions.  */
#ifdef CONFIG_UTF8
		/* struct term_event_keyboard carries UCS-4.
		 * - If the "utf_8_io" option is true or the "charset"
		 *   option refers to UTF-8, then term->utf8_io is true,
		 *   and handle_interlink_event() converts from UTF-8
		 *   to UCS-4.
		 * - Otherwise, handle_interlink_event() converts from
		 *   the codepage specified with the "charset" option
		 *   to UCS-4.  */
		utf8_io = term->utf8_io;
#else
		/* struct term_event_keyboard carries bytes in the
		 * charset of the terminal.
		 * - If the "utf_8_io" option is true, then
		 *   handle_interlink_event() converts from UTF-8 to
		 *   UCS-4, and term_send_ucs() converts from UCS-4 to
		 *   the codepage specified with the "charset" option;
		 *   this codepage cannot be UTF-8.
		 * - Otherwise, handle_interlink_event() passes the
		 *   bytes straight through.  */
		utf8_io = get_opt_bool_tree(term->spec, "utf_8_io");
#endif /* CONFIG_UTF8 */

		/* In UTF-8 byte sequences that have more than one byte, the
		 * first byte is between 0xC0 and 0xFF and the remaining bytes
		 * are between 0x80 and 0xBF.  If there is just one byte, then
		 * it is between 0x00 and 0x7F and it is straight ASCII.
		 * (All 'betweens' are inclusive.) */

		if (interlink->utf8.len) {
			/* A previous call to handle_interlink_event
			 * got a UTF-8 start byte. */

			if (key >= 0x80 && key <= 0xBF && utf8_io) {
				/* This is a UTF-8 continuation byte. */

				interlink->utf8.ucs <<= 6;
				interlink->utf8.ucs |= key & 0x3F;
				if (! --interlink->utf8.len) {
					unicode_val_T u = interlink->utf8.ucs;

					/* UTF-8 allows neither overlong
					 * sequences nor surrogates.  */
					if (u < interlink->utf8.min
					    || is_utf16_surrogate(u))
						u = UCS_REPLACEMENT_CHARACTER;
					term_send_ucs(term, u,
						      term->interlink->utf8.modifier);
				}
				break;

			} else {
				/* The byte sequence for this character
				 * is ending prematurely.  Send
				 * UCS_REPLACEMENT_CHARACTER for the
				 * terminated character, but don't break;
				 * let this byte be handled below. */

				interlink->utf8.len = 0;
				term_send_ucs(term, UCS_REPLACEMENT_CHARACTER,
					      term->interlink->utf8.modifier);
			}
		}

		/* Note: We know that key <= 0xFF. */

		if (key < 0x80 || !utf8_io) {
			/* This byte is not part of a multibyte character
			 * encoding: either it is outside of the ranges for
			 * UTF-8 start and continuation bytes or UTF-8 I/O mode
			 * is disabled. */

#ifdef CONFIG_UTF8
			if (key >= 0 && !utf8_io) {
				/* Not special and UTF-8 mode is disabled:
				 * recode from the terminal charset to UCS-4. */

				key = cp2u(get_terminal_codepage(term), key);
				term_send_ucs(term, key, modifier);
				break;
			}
#endif /* !CONFIG_UTF8 */

			/* It must be special (e.g., F1 or Enter)
			 * or a single-byte UTF-8 character. */
			set_kbd_term_event(&tev, key, modifier);
			term_send_event(term, &tev);
			break;

		} else if ((key & 0xC0) == 0xC0 && (key & 0xFE) != 0xFE) {
			/* This is a UTF-8 start byte. */

			/* Minimum character values for UTF-8 octet
			 * sequences of each length, from RFC 2279.
			 * According to the RFC, UTF-8 decoders should
			 * reject characters that are encoded with
			 * more octets than necessary.  (RFC 3629,
			 * which ELinks does not yet otherwise follow,
			 * tightened the "should" to "MUST".)  */
			static const unicode_val_T min[] = {
				0x00000080, /* ... 0x000007FF with 2 octets */
				0x00000800, /* ... 0x0000FFFF with 3 octets */
				0x00010000, /* ... 0x001FFFFF with 4 octets */
				0x00200000, /* ... 0x03FFFFFF with 5 octets */
				0x04000000  /* ... 0x7FFFFFFF with 6 octets */
			};
			unsigned int mask;
			int len = 0;

			/* Set @len = the number of contiguous 1's
			 * in the most significant bits of the first
			 * octet, i.e. @key.  It is also the number
			 * of octets in the character.  Leave @mask
			 * pointing to the first 0 bit found.  */
			for (mask = 0x80; key & mask; mask >>= 1)
				len++;

			/* This will hold because @key was checked above.  */
			assert(len >= 2 && len <= 6);
			if_assert_failed goto invalid_utf8_start_byte;

			interlink->utf8.min = min[len - 2];
			interlink->utf8.len = len - 1;
			interlink->utf8.ucs = key & (mask - 1);
			interlink->utf8.modifier = modifier;
			break;
		}

invalid_utf8_start_byte:
		term_send_ucs(term, UCS_REPLACEMENT_CHARACTER, modifier);
		break;
	}

	case EVENT_ABORT:
		destroy_terminal(term);
		return 0;

	default:
		ERROR(gettext("Bad event %d"), ilev->ev);
	}

	/* For EVENT_INIT we read a liitle more */
	if (info) return TERMINAL_INFO_SIZE + info->length;
	return sizeof(*ilev);
}

void
in_term(struct terminal *term)
{
	struct terminal_interlink *interlink = term->interlink;
	ssize_t r;
	unsigned char *iq;

	if (!interlink
	    || !interlink->qfreespace
	    || interlink->qfreespace - interlink->qlen > ALLOC_GR) {
		int qlen = interlink ? interlink->qlen : 0;
		int queuesize = ((qlen + ALLOC_GR) & ~(ALLOC_GR - 1));
		int newsize = sizeof(*interlink) + queuesize;

		interlink = mem_realloc(interlink, newsize);
		if (!interlink) {
			destroy_terminal(term);
			return;
		}

		/* Blank the members for the first allocation */
		if (!term->interlink)
			memset(interlink, 0, sizeof(*interlink));

		term->interlink = interlink;
		interlink->qfreespace = queuesize - interlink->qlen;
	}

	iq = interlink->input_queue;
	r = safe_read(term->fdin, iq + interlink->qlen, interlink->qfreespace);
	if (r <= 0) {
		if (r == -1 && errno != ECONNRESET)
			ERROR(gettext("Could not read event: %d (%s)"),
			      errno, (unsigned char *) strerror(errno));

		destroy_terminal(term);
		return;
	}

	interlink->qlen += r;
	interlink->qfreespace -= r;

	while (interlink->qlen >= sizeof(struct interlink_event)) {
		struct interlink_event *ev = (struct interlink_event *) iq;
		int event_size = handle_interlink_event(term, ev);

		/* If the event was not handled save the bytes in the queue for
		 * later in case more stuff is read later. */
		if (!event_size) break;

		/* Acount for the handled bytes */
		interlink->qlen -= event_size;
		interlink->qfreespace += event_size;

		/* If there are no more bytes to handle stop else move next
		 * event bytes to the front of the queue. */
		if (!interlink->qlen) break;
		memmove(iq, iq + event_size, interlink->qlen);
	}
}
