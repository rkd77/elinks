#ifndef EL__BFU_LEDS_H
#define EL__BFU_LEDS_H

#ifdef CONFIG_LEDS

#include "main/module.h"
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
	/* Private data. */
	/* 32 bits */
	unsigned int used__:1;
	unsigned int value_changed__:1;
	unsigned int value__:8;
};

/* Per-session led panel structure. */
struct led_panel {
	struct led leds[LEDS_COUNT];
};


extern struct module leds_module;

void init_led_panel(struct led_panel *leds);

void draw_leds(struct session *ses);

void menu_leds_info(struct terminal *term, void *xxx, void *xxxx);

struct led *register_led(struct session *ses, int number);
void unregister_led(struct led *);
void set_led_value(struct led *led, unsigned char value);
unsigned char get_led_value(struct led *led);
void unset_led_value(struct led *led);

#endif
#endif
