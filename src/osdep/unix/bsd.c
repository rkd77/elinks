/* BSD mouse system-specific routines. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#ifdef CONFIG_SYSMOUSE
#ifdef HAVE_SYS_CONSIO_H
#include <sys/consio.h>
#include <sys/fbio.h>
#else
#include <machine/console.h>
#endif	/* HAVE_SYS_CONSIO_H */
#endif	/* CONFIG_SYSMOUSE */

#include "elinks.h"

#include "main/select.h"
#include "osdep/osdep.h"
#include "osdep/signals.h"
#include "terminal/event.h"
#include "terminal/mouse.h"


#if defined(CONFIG_SYSMOUSE) && defined(CONFIG_MOUSE)

struct sysmouse_spec {
	void *itrm;
	int cheight;
	int cwidth;
	void (*fn)(void *, unsigned char *, int);
};

static void
sysmouse_handler(void *data)
{
	static struct interlink_event_mouse prev_mouse;
	static int prev_buttons;
	struct sysmouse_spec *sp = data;
	void *itrm = sp->itrm;
	int fd = get_output_handle();
	int buttons, change;
	int extended_button;
	mouse_info_t mi;
	struct interlink_event_mouse mouse;
	struct interlink_event ev;

	mi.operation = MOUSE_GETINFO;
	if (ioctl(fd, CONS_MOUSECTL, &mi) == -1) return;
	mouse.x = int_max(mi.u.data.x / sp->cwidth, 0);
	mouse.y = int_max(mi.u.data.y / sp->cheight, 0);

	/* for cosmetic bug in syscons.c on FreeBSD 3.3/3.4 */
#ifdef HAVE_MACHINE_CONSOLE_H
	mi.operation = MOUSE_HIDE;
	ioctl(fd, CONS_MOUSECTL, &mi);
	mi.operation = MOUSE_SHOW;
	ioctl(fd, CONS_MOUSECTL, &mi);
#endif
	buttons = mi.u.data.buttons & 7;
	change = (mouse.x != prev_mouse.x || mouse.y != prev_mouse.y);
	prev_mouse = mouse;
	/* It's horrible. */
	switch (buttons) {
	case 0:
		switch (prev_buttons) {
		case 0:
			extended_button = mi.u.data.buttons & 24;
			if (!extended_button) return;
			if (extended_button & 8) mouse.button = B_WHEEL_UP;
			else mouse.button = B_WHEEL_DOWN;
			break;
		case 1:
		case 3:
		case 5:
		case 7:
			mouse.button = B_LEFT | B_UP;
			break;
		case 2:
		case 6:
			mouse.button = B_MIDDLE | B_UP;
			break;
		case 4:
			mouse.button = B_RIGHT | B_UP;
			break;
		}
		break;
	case 1:
	case 3:
	case 5:
	case 7:
		switch (prev_buttons) {
		case 0:
		case 2:
		case 4:
		case 6:
			mouse.button = B_LEFT | B_DOWN;
			break;
		case 1:
		case 3:
		case 5:
		case 7:
			if (change)
				mouse.button = B_LEFT | B_DRAG;
			else mouse.button = B_LEFT | B_DOWN;
			break;
		}
		break;
	case 2:
	case 6:
		switch (prev_buttons) {
		case 1:
		case 3:
		case 5:
		case 7:
			mouse.button = B_LEFT | B_UP;
			break;
		case 0:
		case 4:
			mouse.button = B_MIDDLE | B_DOWN;
			break;
		case 2:
		case 6:
			if (change)
				mouse.button = B_MIDDLE | B_DRAG;
			else mouse.button = B_MIDDLE | B_DOWN;
			break;
		}
		break;
	case 4:
		switch (prev_buttons) {
		case 1:
		case 3:
		case 5:
		case 7:
			mouse.button = B_LEFT | B_UP;
			break;
		case 2:
		case 6:
			mouse.button = B_MIDDLE | B_UP;
			break;
		case 0:
			mouse.button = B_RIGHT | B_DOWN;
			break;
		case 4:
			if (change)
				mouse.button = B_RIGHT | B_DRAG;
			else mouse.button = B_RIGHT | B_DOWN;
			break;
		}
		break;
	}

	prev_buttons = buttons;
	set_mouse_interlink_event(&ev, mouse.x, mouse.y, mouse.button);
	sp->fn(itrm, (unsigned char *)&ev, sizeof(ev));
}

static void
sysmouse_signal_handler(void *data)
{
	register_bottom_half(sysmouse_handler, data);
}

void *
handle_mouse(int cons, void (*fn)(void *, unsigned char *, int),
	     void *data)
{
	static struct sysmouse_spec mouse_spec;
	video_info_t vi;
	mouse_info_t mi;
	int fd = get_output_handle();

	if (is_xterm()) return NULL;
	mouse_spec.itrm = data;
	mouse_spec.fn = fn;

	if (ioctl(fd, FBIO_GETMODE, &vi.vi_mode) != -1 &&
		ioctl(fd, FBIO_MODEINFO, &vi) != -1) {
		mouse_spec.cwidth = vi.vi_cwidth;
		mouse_spec.cheight = vi.vi_cheight;
	} else {
		return NULL;
	}

	install_signal_handler(SIGUSR2, NULL, NULL, 0);
	mi.operation = MOUSE_MODE;
	mi.u.mode.mode = 0;
	mi.u.mode.signal = SIGUSR2;
	if (ioctl(fd, CONS_MOUSECTL, &mi) != -1) {
		install_signal_handler(SIGUSR2,
		(void (*)(void *))sysmouse_signal_handler, &mouse_spec, 0);
		mi.operation = MOUSE_SHOW;
		ioctl(fd, CONS_MOUSECTL, &mi);
		return &mouse_spec;
	} else {
		return NULL;
	}
}

void
unhandle_mouse(void *data)
{
	if (data) {
		mouse_info_t mi;
		int fd = get_output_handle();

		mi.operation = MOUSE_MODE;
		mi.u.mode.mode = 0;
		mi.u.mode.signal = 0;
		install_signal_handler(SIGUSR2, NULL, NULL, 0);
		ioctl(fd, CONS_MOUSECTL, &mi);
	}
}

void
suspend_mouse(void *data)
{
	unhandle_mouse(data);
}

void
resume_mouse(void *data)
{
	if (data) {
		mouse_info_t mi;
		int fd = get_output_handle();

		mi.operation = MOUSE_MODE;
		mi.u.mode.mode = 0;
		mi.u.mode.signal = SIGUSR2;;
		install_signal_handler(SIGUSR2,
		(void (*)(void *))sysmouse_signal_handler, data, 0);
		ioctl(fd, CONS_MOUSECTL, &mi);
	}
}

#endif
