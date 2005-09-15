/* $Id: leds.h,v 1.17.2.1 2005/01/29 01:39:15 jonas Exp $ */

#ifndef EL__BFU_LEDS_H
#define EL__BFU_LEDS_H

#ifdef CONFIG_LEDS

#include "modules/module.h"
#include "util/color.h"

struct session;
struct terminal;

/* TODO: Variable count! */
#define LEDS_COUNT	6

/* We use struct in order to at least somehow 'authorize' client to use certain
 * LED, preventing possible mess i.e. with conflicting patches or Lua scripts.
 */

/* See header of bfu/leds.c for LEDs assignment. If you are planning to use
 * some LED in your script/patch, please tell us on the list so that we can
 * register the LED for you. Always check latest sources for actual LED
 * assignment scheme in order to prevent conflicts. */

struct led {
	int number;
	unsigned char value;

	/* Private data. */
	int used__;
};

/* Per-session led panel structure. */
struct led_panel {
	struct led leds[LEDS_COUNT];
	unsigned char leds_backup[LEDS_COUNT];
};


extern struct module leds_module;

void init_led_panel(struct led_panel *leds);

void draw_leds(struct session *ses);

void menu_leds_info(struct terminal *term, void *xxx, void *xxxx);

struct led *register_led(struct session *ses, int number);
void unregister_led(struct led *);

#endif
#endif
