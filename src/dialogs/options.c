/* Options dialogs */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/menu.h"
#include "config/conf.h"
#include "config/options.h"
#include "dialogs/options.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "osdep/osdep.h"
#include "session/session.h"
#include "terminal/color.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/memlist.h"


static void
display_codepage(struct terminal *term, void *name_, void *xxx)
{
	unsigned char *name = name_;
	struct option *opt = get_opt_rec(term->spec, "charset");
	int index = get_cp_index(name);

	assertm(index != -1, "%s", name);

	if (opt->value.number != index) {
		opt->value.number = index;
		option_changed(NULL, opt);
	}

	cls_redraw_all_terminals();
}

void
charset_list(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;
	int i, items;
	int sel = 0;
	const unsigned char *const sel_mime = get_cp_mime_name(
		get_terminal_codepage(term));
	struct menu_item *mi = new_menu(FREE_LIST);

	if (!mi) return;

	for (i = 0, items = 0; ; i++) {
		unsigned char *name = get_cp_name(i);

		if (!name) break;

#ifndef CONFIG_UTF8
		if (is_cp_utf8(i)) continue;
#endif /* CONFIG_UTF8 */

		/* Map the "System" codepage to the underlying one.
		 * A pointer comparison might suffice here but this
		 * code is not time-critical.  */
		if (strcmp(sel_mime, get_cp_mime_name(i)) == 0)
			sel = items;
		items++;
		add_to_menu(&mi, name, NULL, ACT_MAIN_NONE,
			    display_codepage, get_cp_config_name(i), 0);
	}

	do_menu_selected(term, mi, ses, sel, 0);
}


/* TODO: Build this automagically. But that will need gettextted options
 * captions not to lose translations and so on. 0.5 stuff or even later.
 * --pasky */

enum termopt {
	TERM_OPT_TYPE = 0,
	TERM_OPT_M11_HACK,
	TERM_OPT_RESTRICT_852,
	TERM_OPT_BLOCK_CURSOR,
	TERM_OPT_COLORS,
	TERM_OPT_UTF_8_IO,
	TERM_OPT_TRANSPARENCY,
	TERM_OPT_UNDERLINE,

	TERM_OPTIONS,
};

static struct option_resolver resolvers[] = {
	{ TERM_OPT_TYPE,	 "type"		},
	{ TERM_OPT_M11_HACK,	 "m11_hack"	},
	{ TERM_OPT_RESTRICT_852, "restrict_852"	},
	{ TERM_OPT_BLOCK_CURSOR, "block_cursor"	},
	{ TERM_OPT_COLORS,	 "colors"	},
	{ TERM_OPT_TRANSPARENCY, "transparency"	},
	{ TERM_OPT_UTF_8_IO,	 "utf_8_io"	},
	{ TERM_OPT_UNDERLINE,	 "underline"	},
};

static widget_handler_status_T
push_ok_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	struct terminal *term = dlg_data->win->term;
	union option_value *values = dlg_data->dlg->udata;

	update_dialog_data(dlg_data);

	commit_option_values(resolvers, term->spec, values, TERM_OPTIONS);

	if (button->widget->handler == push_ok_button)
		return cancel_dialog(dlg_data, button);

	return EVENT_PROCESSED;
}

static widget_handler_status_T
push_save_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	push_ok_button(dlg_data, button);
	write_config(dlg_data->win->term);

	return EVENT_PROCESSED;
}

#if	defined(CONFIG_88_COLORS)
#define	RADIO_88 1
#else
#define	RADIO_88 0
#endif

#if	defined(CONFIG_256_COLORS)
#define	RADIO_256 1
#else
#define	RADIO_256 0
#endif

#if	defined(CONFIG_TRUE_COLOR)
#define	RADIO_TRUE 1
#else
#define	RADIO_TRUE 0
#endif

#define TERMOPT_WIDGETS_COUNT (19 + RADIO_88 + RADIO_256 + RADIO_TRUE)

#define TERM_OPTION_VALUE_SIZE (sizeof(union option_value) * TERM_OPTIONS)

void
terminal_options(struct terminal *term, void *xxx, struct session *ses)
{
	/* [gettext_accelerator_context(terminal_options)] */
	struct dialog *dlg;
	union option_value *values;
	int anonymous = get_cmd_opt_bool("anonymous");
	unsigned char help_text[MAX_STR_LEN], *text;
	size_t help_textlen = 0;
	size_t add_size = TERM_OPTION_VALUE_SIZE;

	/* XXX: we don't display help text when terminal height is too low,
	 * because then user can't change values.
	 * This should be dropped when we'll have scrollable dialog boxes.
	 * --Zas */
	if (term->height > 30) {
		snprintf(help_text, sizeof(help_text) - 3 /* 2 '\n' + 1 '\0' */,
			 _("The environmental variable TERM is set to '%s'.\n"
			"\n"
			"ELinks maintains separate sets of values for these options\n"
			"and chooses the appropriate set based on the value of TERM.\n"
			"This allows you to configure the settings appropriately for\n"
			"each terminal in which you run ELinks.", term),
			 term->spec->name);

		help_textlen = strlen(help_text);

		/* Two newlines are needed to get a blank line between the help text and
		 * the first group of widgets. */
		help_text[help_textlen++] = '\n';
		help_text[help_textlen++] = '\n';
	}

	help_text[help_textlen++] = '\0';

	add_size += help_textlen;

	dlg = calloc_dialog(TERMOPT_WIDGETS_COUNT, add_size);
	if (!dlg) return;

	values = (union option_value *) get_dialog_offset(dlg, TERMOPT_WIDGETS_COUNT);
	checkout_option_values(resolvers, term->spec, values, TERM_OPTIONS);

	dlg->title = _("Terminal options", term);
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.padding_top = 1;
	dlg->udata = values;

	text = ((unsigned char *) values) + TERM_OPTION_VALUE_SIZE;
	memcpy(text, help_text, help_textlen);
	add_dlg_text(dlg, text, ALIGN_LEFT, 1);

	add_dlg_text(dlg, _("Frame handling:", term), ALIGN_LEFT, 1);
	add_dlg_radio(dlg, _("No frames", term), 1, TERM_DUMB, &values[TERM_OPT_TYPE].number);
	add_dlg_radio(dlg, _("VT 100 frames", term), 1, TERM_VT100, &values[TERM_OPT_TYPE].number);
	add_dlg_radio(dlg, _("Linux or OS/2 frames", term), 1, TERM_LINUX, &values[TERM_OPT_TYPE].number);
	add_dlg_radio(dlg, _("FreeBSD frames", term), 1, TERM_FREEBSD, &values[TERM_OPT_TYPE].number);
	add_dlg_radio(dlg, _("KOI8-R frames", term), 1, TERM_KOI8, &values[TERM_OPT_TYPE].number);

	add_dlg_text(dlg, _("Color mode:", term), ALIGN_LEFT, 1);
	add_dlg_radio(dlg, _("No colors (mono)", term), 2, COLOR_MODE_MONO, &values[TERM_OPT_COLORS].number);
	add_dlg_radio(dlg, _("16 colors", term), 2, COLOR_MODE_16, &values[TERM_OPT_COLORS].number);
#ifdef CONFIG_88_COLORS
	add_dlg_radio(dlg, _("88 colors", term), 2, COLOR_MODE_88, &values[TERM_OPT_COLORS].number);
#endif
#ifdef CONFIG_256_COLORS
	add_dlg_radio(dlg, _("256 colors", term), 2, COLOR_MODE_256, &values[TERM_OPT_COLORS].number);
#endif
#ifdef CONFIG_TRUE_COLOR
	add_dlg_radio(dlg, _("true color", term), 2, COLOR_MODE_TRUE_COLOR, &values[TERM_OPT_COLORS].number);
#endif
	add_dlg_checkbox(dlg, _("Switch fonts for line drawing", term), &values[TERM_OPT_M11_HACK].number);
	add_dlg_checkbox(dlg, _("Restrict frames in cp850/852", term), &values[TERM_OPT_RESTRICT_852].number);
	add_dlg_checkbox(dlg, _("Block cursor", term), &values[TERM_OPT_BLOCK_CURSOR].number);
	add_dlg_checkbox(dlg, _("Transparency", term), &values[TERM_OPT_TRANSPARENCY].number);
	add_dlg_checkbox(dlg, _("Underline", term), &values[TERM_OPT_UNDERLINE].number);
	add_dlg_checkbox(dlg, _("UTF-8 I/O", term), &values[TERM_OPT_UTF_8_IO].number);

	add_dlg_button(dlg, _("~OK", term), B_ENTER, push_ok_button, NULL);
	if (!anonymous)
		add_dlg_button(dlg, _("Sa~ve", term), B_ENTER, push_save_button, NULL);
	add_dlg_button(dlg, _("~Cancel", term), B_ESC, cancel_dialog, NULL);

	add_dlg_end(dlg, TERMOPT_WIDGETS_COUNT - anonymous);

	do_dialog(term, dlg, getml(dlg, (void *) NULL));
}

#ifdef CONFIG_NLS
static void
menu_set_language(struct terminal *term, void *pcp_, void *xxx)
{
	int pcp = (long) pcp_;

	set_language(pcp);
	cls_redraw_all_terminals();
}
#endif

void
menu_language_list(struct terminal *term, void *xxx, void *ses)
{
#ifdef CONFIG_NLS
	int i;
	struct menu_item *mi = new_menu(FREE_LIST);

	if (!mi) return;
	for (i = 0; languages[i].name; i++) {
		add_to_menu(&mi, languages[i].name, language_to_iso639(i), ACT_MAIN_NONE,
			    menu_set_language, (void *) (long) i, 0);
	}

	do_menu_selected(term, mi, ses, current_language, 0);
#endif
}


/* FIXME: This doesn't in fact belong here at all. --pasky */

static unsigned char width_str[4];
static unsigned char height_str[4];

static void
push_resize_button(void *data)
{
	struct terminal *term = data;
	unsigned char str[MAX_STR_LEN];

	snprintf(str, sizeof(str), "%s,%s,%d,%d",
		 width_str, height_str, term->width, term->height);

	do_terminal_function(term, TERM_FN_RESIZE, str);
}

/* menu_func_T */
void
resize_terminal_dialog(struct terminal *term)
{
	/* [gettext_accelerator_context(resize_terminal_dialog)] */
	struct dialog *dlg;
	int width = int_min(term->width, 999);
	int height = int_min(term->height, 999);

	if (!can_resize_window(term->environment))
		return;

	ulongcat(width_str, NULL, width, 3, ' ');
	ulongcat(height_str, NULL, height, 3, ' ');

#define RESIZE_WIDGETS_COUNT 4
	dlg = calloc_dialog(RESIZE_WIDGETS_COUNT, 0);
	if (!dlg) return;

	dlg->title = _("Resize terminal", term);
	dlg->layouter = group_layouter;

	add_dlg_field(dlg, _("Width=",term), 1, 999, check_number, 4, width_str, NULL);
	add_dlg_field(dlg, _("Height=",term), 1, 999, check_number, 4, height_str, NULL);

	add_dlg_ok_button(dlg, _("~OK", term), B_ENTER, push_resize_button, term);
	add_dlg_button(dlg, _("~Cancel", term), B_ESC, cancel_dialog, NULL);

	add_dlg_end(dlg, RESIZE_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, (void *) NULL));
}
