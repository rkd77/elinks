/* UNIX system-specific routines. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "osdep/system.h"

#if defined(CONFIG_GPM) && defined(HAVE_GPM_H)
#include <gpm.h>
#endif

#include "elinks.h"

#include "main/select.h"
#include "osdep/unix/unix.h"
#include "osdep/osdep.h"
#include "terminal/event.h"
#include "terminal/mouse.h"
#include "util/error.h"
#include "util/memory.h"


#if defined(CONFIG_GPM) && defined(CONFIG_MOUSE)

struct gpm_mouse_spec {
	int h;
	int cons;
	void (*fn)(void *, unsigned char *, int);
	void *data;
};

static void
gpm_mouse_in(struct gpm_mouse_spec *gms)
{
	Gpm_Event gev;
	struct interlink_event ev;
	struct interlink_event_mouse mouse;

	if (Gpm_GetEvent(&gev) <= 0) {
		clear_handlers(gms->h);
		return;
	}

	mouse.x = int_max(gev.x - 1, 0);
	mouse.y = int_max(gev.y - 1, 0);
	Gpm_DrawPointer(gev.x, gev.y, 1);

	if (gev.buttons & GPM_B_LEFT)
		mouse.button = B_LEFT;
	else if (gev.buttons & GPM_B_MIDDLE)
		mouse.button = B_MIDDLE;
	else if (gev.buttons & GPM_B_RIGHT)
		mouse.button = B_RIGHT;
	/* Somehow, we don't get those events more frequently than every
	 * second or so. Duh. Well, gpm. */
	else if (gev.wdy < 0)
		mouse.button = B_WHEEL_DOWN;
	else if (gev.wdy > 0)
		mouse.button = B_WHEEL_UP;
	else
		return;

	if (gev.type & GPM_DOWN)
		mouse.button |= B_DOWN;
	else if (gev.type & GPM_UP)
		mouse.button |= B_UP;
	else if (gev.type & GPM_DRAG)
		mouse.button |= B_DRAG;
	else if (gev.wdy != 0)
		mouse.button |= B_DOWN;
	else
		return;

	set_mouse_interlink_event(&ev, mouse.x, mouse.y, mouse.button);
	gms->fn(gms->data, (char *) &ev, sizeof(ev));
}

static int
init_mouse(int cons, int suspend)
{
	Gpm_Connect conn;

	/* We want to get even move events because of the wheels. */
	conn.eventMask = suspend ? 0 : ~0;
	conn.defaultMask = suspend ? ~0 : 0;
	conn.minMod = suspend ? ~0 : 0;
	conn.maxMod = suspend ? ~0 : 0;
	gpm_visiblepointer = 1;

	return Gpm_Open(&conn, cons);
}

static int
done_mouse(void)
{
	return Gpm_Close();
}

void *
handle_mouse(int cons, void (*fn)(void *, unsigned char *, int),
	     void *data)
{
	int h;
	struct gpm_mouse_spec *gms;

	h = init_mouse(cons, 0);
	if (h < 0) return NULL;

	gms = mem_alloc(sizeof(*gms));
	if (!gms) return NULL;
	gms->h = h;
	gms->cons = cons;
	gms->fn = fn;
	gms->data = data;
	set_handlers(h, (select_handler_T) gpm_mouse_in, NULL, NULL, gms);

	return gms;
}

void
unhandle_mouse(void *h)
{
	struct gpm_mouse_spec *gms = h;

	if (!gms) return;

	clear_handlers(gms->h);
	mem_free(gms);
	done_mouse();
}

void
suspend_mouse(void *h)
{
	struct gpm_mouse_spec *gms = h;

	if (!gms) return;

	gms->h = init_mouse(gms->cons, 1);
	if (gms->h < 0) return;

	clear_handlers(gms->h);
}

void
resume_mouse(void *h)
{
	struct gpm_mouse_spec *gms = h;

	if (!gms) return;

	gms->h = init_mouse(gms->cons, 0);
	if (gms->h < 0) return;

	set_handlers(gms->h, (select_handler_T) gpm_mouse_in, NULL, NULL, gms);
}

#endif
