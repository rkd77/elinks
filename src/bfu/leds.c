/* These cute LightEmittingDiode-like indicators. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "elinks.h"

#include "bfu/leds.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "main/timer.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/error.h"
#include "util/time.h"
#include "viewer/timer.h"

#define LEDS_REFRESH_DELAY	((milliseconds_T) 100)

/* Current leds allocation:
 * 0 - SSL connection indicator
 * 1 - Insert-mode indicator
 * 2 - JavaScript Error indicator
 * 3 - JavaScript pop-up blocking indicator
 * 4 - unused, reserved for Lua
 * 5 - unused */

/* XXX: Currently, the leds toggling is quite hackish, some more work should go
 * to it (ie. some led hooks called in sync_leds() to light the leds
 * dynamically. --pasky */

/* Always reset led to '-' when not used anymore. */

/* If we would do real protection, we would do this as array of pointers. This
 * way someone can just get any struct led and add/subscribe appropriate struct
 * led for his control; however, I bet on programmers' responsibility rather,
 * and hope that everyone will abide the "rules". */

static int timer_duration_backup = 0;

static timer_id_T redraw_timer = TIMER_ID_UNDEF;
static int drawing = 0;

static void redraw_leds(void *);

enum led_option {
	LEDS_CLOCK_TREE,
	LEDS_CLOCK_ENABLE,
	LEDS_CLOCK_FORMAT,
	LEDS_CLOCK_ALIAS,

	LEDS_PANEL_TREE,
	LEDS_PANEL_ENABLE,

	LEDS_OPTIONS,
};

static struct option_info led_options[] = {
	INIT_OPT_TREE("ui", N_("Clock"),
		"clock", 0, N_("Digital clock in the status bar.")),

	INIT_OPT_BOOL("ui.clock", N_("Enable"),
		"enable", 0, 0,
		N_("Whether to display a digital clock in the status bar.")),

	INIT_OPT_STRING("ui.clock", N_("Format"),
		"format", 0, "[%H:%M]",
		N_("Format string for the digital clock. See the strftime(3) "
		"manpage for details.")),

	/* Compatibility alias. Added: 2004-04-22, 0.9.CVS. */
	INIT_OPT_ALIAS("ui.timer", "clock", 0, "ui.clock"),


	INIT_OPT_TREE("ui", N_("LEDs"),
		"leds", 0,
		N_("LEDs (visual indicators) options.")),

	INIT_OPT_BOOL("ui.leds", N_("Enable"),
		"enable", 0, 1,
		N_("Enable LEDs. These visual indicators will inform you "
		"about various states.")),

	NULL_OPTION_INFO,
};

#define get_opt_leds(which)		led_options[(which)].option.value
#define get_leds_clock_enable()		get_opt_leds(LEDS_CLOCK_ENABLE).number
#define get_leds_clock_format()		get_opt_leds(LEDS_CLOCK_FORMAT).string
#define get_leds_panel_enable()		get_opt_leds(LEDS_PANEL_ENABLE).number

void
init_leds(struct module *module)
{
	timer_duration_backup = 0;

	/* We can't setup timer here, because we may not manage to startup in
	 * 100ms and we will get to problems when we will call draw_leds() on
	 * uninitialized terminal. So, we will wait for draw_leds(). */
}

void
done_leds(struct module *module)
{
	kill_timer(&redraw_timer);
}

void
set_led_value(struct led *led, unsigned char value)
{
	if (value != led->value__) {
		led->value__ = value;
		led->value_changed__ = 1;
	}
}

void
unset_led_value(struct led *led)
{
	set_led_value(led, '-');
}


void
init_led_panel(struct led_panel *leds)
{
	int i;

	for (i = 0; i < LEDS_COUNT; i++) {
		leds->leds[i].used__ = 0;
		unset_led_value(&leds->leds[i]);
	}
}

static int
draw_timer(struct terminal *term, int xpos, int ypos, struct color_pair *color)
{
	unsigned char s[64];
	int i, length;

	snprintf(s, sizeof(s), "[%d]", get_timer_duration());
	length = strlen(s);

	for (i = length - 1; i >= 0; i--)
		draw_char(term, xpos - (length - i), ypos, s[i], 0, color);

	return length;
}

#ifdef HAVE_STRFTIME
static int
draw_clock(struct terminal *term, int xpos, int ypos, struct color_pair *color)
{
	unsigned char s[64];
	time_t curtime = time(NULL);
	struct tm *loctime = localtime(&curtime);
	int i, length;

	length = strftime(s, sizeof(s), get_leds_clock_format(), loctime);
	s[length] = '\0';
	for (i = length - 1; i >= 0; i--)
		draw_char(term, xpos - (length - i), ypos, s[i], 0, color);

	return length;
}
#endif

void
draw_leds(struct session *ses)
{
	struct terminal *term = ses->tab->term;
	struct color_pair *led_color = NULL;
	int i;
	int xpos = term->width - LEDS_COUNT - 3;
	int ypos = term->height - 1;

	term->leds_length = 0;

	/* This should be done elsewhere, but this is very nice place where we
	 * could do that easily. */
	if (get_opt_int("ui.timer.enable") == 2) {
		led_color = get_bfu_color(term, "status.status-text");
		if (!led_color) goto end;

		term->leds_length += draw_timer(term, xpos, ypos, led_color);
	}

	if (!get_leds_panel_enable()) return;

	if (!led_color) {
		led_color = get_bfu_color(term, "status.status-text");
		if (!led_color) goto end;
	}

#ifdef HAVE_STRFTIME
	if (get_leds_clock_enable()) {
		term->leds_length += draw_clock(term, xpos - term->leds_length, ypos, led_color);
	}
#endif

	/* We must shift the whole thing by one char to left, because we don't
	 * draft the char in the right-down corner :(. */

	draw_char(term, xpos, ypos, '[', 0, led_color);

	for (i = 0; i < LEDS_COUNT; i++) {
		struct led *led = &ses->status.leds.leds[i];

		draw_char(term, xpos + i + 1, ypos, led->value__, 0, led_color);
		led->value_changed__ = 0;
	}

	draw_char(term, xpos + LEDS_COUNT + 1, ypos, ']', 0, led_color);

	term->leds_length += LEDS_COUNT + 2;

end:
	/* Redraw each 100ms. */
	if (!drawing && redraw_timer == TIMER_ID_UNDEF)
		install_timer(&redraw_timer, LEDS_REFRESH_DELAY, redraw_leds, NULL);
}

/* Determine if leds redrawing is necessary. Returns non-zero if so. */
static int
sync_leds(struct session *ses)
{
	int i;
	int timer_duration;

#ifdef HAVE_STRFTIME
	/* Check if clock was enabled and update if needed. */
	if (get_leds_clock_enable()) {
		/* We _always_ update when clock is enabled
		 * Not perfect. --Zas */
		return 1;
	}
#endif

	for (i = 0; i < LEDS_COUNT; i++) {
		struct led *led = &ses->status.leds.leds[i];

		if (led->value_changed__)
			return 1;
	}

	/* Check if timer was updated. */
	timer_duration = get_timer_duration();
	if (timer_duration_backup != timer_duration) {
		timer_duration_backup = timer_duration;
		return 1;
	}

	return 0;
}

/* Timer callback for @redraw_timer.  As explained in @install_timer,
 * this function must erase the expired timer ID from all variables.  */
static void
redraw_leds(void *xxx)
{
	struct session *ses;

	if (!get_leds_panel_enable()
	    && get_opt_int("ui.timer.enable") != 2) {
		redraw_timer = TIMER_ID_UNDEF;
		return;
	}

	install_timer(&redraw_timer, LEDS_REFRESH_DELAY, redraw_leds, NULL);
	/* The expired timer ID has now been erased.  */

	if (drawing) return;
	drawing = 1;

	foreach (ses, sessions) {
		if (!sync_leds(ses))
			continue;
		redraw_terminal(ses->tab->term);
		draw_leds(ses);
	}
	drawing = 0;
}

void
menu_leds_info(struct terminal *term, void *xxx, void *xxxx)
{
	/* If LEDs ever get more dynamic we might have to change this, but it
	 * should do for now. --jonas */
	info_box(term, MSGBOX_FREE_TEXT | MSGBOX_SCROLLABLE,
	 	 N_("LED indicators"), ALIGN_LEFT,
		 msg_text(term, N_("What the different LEDs indicate:\n"
		 	"\n"
			"[SIJP--]\n"
			" |||||`- Unused\n"
			" ||||`-- Unused\n"
			" |||`--- A JavaScript pop-up window was blocked\n"
			" ||`---- A JavaScript error has occurred\n"
			" |`----- The state of insert mode for text-input form-fields\n"
			" |       'i' means modeless, 'I' means insert mode is on\n"
			" `------ Whether an SSL connection was used\n"
			"\n"
			"'-' generally indicates that the LED is off.")));
}


struct led *
register_led(struct session *ses, int number)
{
	struct led *led;

	if (number >= LEDS_COUNT || number < 0)
		return NULL;

	led = &ses->status.leds.leds[number];
	if (led->used__)
		return NULL;

	led->used__ = 1;

	return led;
}

void
unregister_led(struct led *led)
{
	assertm(led->used__, "Attempted to unregister unused led!");
	led->used__ = 0;
	unset_led_value(led);
}

struct module leds_module = struct_module(
	/* name: */		N_("LED indicators"),
	/* options: */		led_options,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_leds,
	/* done: */		done_leds
);
