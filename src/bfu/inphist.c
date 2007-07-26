/* Input history for input fields. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bfu/menu.h"
#include "config/home.h"
#include "config/options.h"
#include "dialogs/menu.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/secsave.h"


static void
tab_compl_n(struct dialog_data *dlg_data, unsigned char *item, int len)
{
	struct widget_data *widget_data = selected_widget(dlg_data);

	assert(widget_is_textfield(widget_data));

	int_upper_bound(&len, widget_data->widget->datalen - 1);
	memcpy(widget_data->cdata, item, len);
	widget_data->cdata[len] = 0;
	widget_data->info.field.cpos = len;
	widget_data->info.field.vpos = 0;

	redraw_dialog(dlg_data, 1);
}

static void
tab_compl(struct dialog_data *dlg_data, unsigned char *item)
{
	tab_compl_n(dlg_data, item, strlen(item));
}

/* menu_func_T */
static void
menu_tab_compl(struct terminal *term, void *item_, void *dlg_data_)
{
	unsigned char *item = item_;
	struct dialog_data *dlg_data = dlg_data_;

	tab_compl_n(dlg_data, item, strlen(item));
}

/* Complete to last unambiguous character, and display menu for all possible
 * further completions. */
void
do_tab_compl(struct dialog_data *dlg_data,
	     LIST_OF(struct input_history_entry) *history)
{
	struct terminal *term = dlg_data->win->term;
	struct widget_data *widget_data = selected_widget(dlg_data);
	int cpos = widget_data->info.field.cpos;
	int n = 0;
	struct input_history_entry *entry;
	struct menu_item *items = new_menu(FREE_LIST | NO_INTL);

	if (!items) return;

	foreach (entry, *history) {
		if (strncmp(widget_data->cdata, entry->data, cpos))
			continue;

		add_to_menu(&items, entry->data, NULL, ACT_MAIN_NONE,
			    menu_tab_compl, entry->data, 0);
		n++;
	}

	if (n > 1) {
		do_menu_selected(term, items, dlg_data, n - 1, 0);
	} else {
		if (n == 1) tab_compl(dlg_data, items->data);
		mem_free(items);
	}
}

/* Return the length of the common substring from the starts
 * of the two strings a and b. */
static inline int
strcommonlen(unsigned char *a, unsigned char *b)
{
	unsigned char *start = a;

	while (*a && *a == *b)
		++a, ++b;

	return a - start;
}

/* Complete to the last unambiguous character. Eg., I've been to google.com,
 * google.com/search?q=foo, and google.com/search?q=bar.  This function then
 * completes `go' to `google.com' and `google.com/' to `google.com/search?q='.
 */
void
do_tab_compl_unambiguous(struct dialog_data *dlg_data,
			 LIST_OF(struct input_history_entry) *history)
{
	struct string completion;
	struct widget_data *widget_data = selected_widget(dlg_data);
	int base_len = widget_data->info.field.cpos;
	/* Maximum number of characters in a match. Characters after this
	 * position are varying in other matches. */
	int longest_common_match = 0;
	unsigned char *match = NULL;
	struct input_history_entry *entry;

	foreach (entry, *history) {
		unsigned char *cur = entry->data;
		int cur_len = strcommonlen(cur, match ? match
		                                      : widget_data->cdata);

		/* Throw away it away if it isn't even as long as what the user
		 * entered. */
		if (cur_len < base_len)
			continue;

		if (!match) {
			/* This is the first match, so its length is the maximum
			 * for any future matches. */
			longest_common_match = strlen(entry->data);
			match = entry->data;
		} else if (cur_len < longest_common_match) {
			/* The current match has a shorter substring in common
			 * with the previous candidates, so the common substring
			 * shrinks. */
			longest_common_match = cur_len;
		}
	}

	if (!match) return;

	if (!init_string(&completion)) return;

	add_bytes_to_string(&completion, match, longest_common_match);
	add_to_string(&completion, widget_data->cdata
	                            + widget_data->info.field.cpos);

	tab_compl_n(dlg_data, completion.source, completion.length);

	done_string(&completion);
}


/* menu_func_T */
static void
set_complete_file_menu(struct terminal *term, void *filename_, void *dlg_data_)
{
	struct dialog_data *dlg_data = dlg_data_;
	struct widget_data *widget_data = selected_widget(dlg_data);
	unsigned char *filename = filename_;
	int filenamelen;

	assert(widget_is_textfield(widget_data));

	filenamelen = int_min(widget_data->widget->datalen - 1, strlen(filename));
	memcpy(widget_data->cdata, filename, filenamelen);

	widget_data->cdata[filenamelen] = 0;
	widget_data->info.field.cpos = filenamelen;
	widget_data->info.field.vpos = 0;

	mem_free(filename);

	redraw_dialog(dlg_data, 1);
}

/* menu_func_T */
static void
tab_complete_file_menu(struct terminal *term, void *path_, void *dlg_data_)
{
	struct dialog_data *dlg_data = dlg_data_;
	unsigned char *path = path_;

	auto_complete_file(term, 0 /* no_elevator */, path,
			   set_complete_file_menu, tab_complete_file_menu,
			   dlg_data);
}

void
do_tab_compl_file(struct dialog_data *dlg_data,
		  LIST_OF(struct input_history_entry) *history)
{
	struct widget_data *widget_data = selected_widget(dlg_data);

	if (get_cmd_opt_bool("anonymous"))
		return;

	tab_complete_file_menu(dlg_data->win->term, widget_data->cdata, dlg_data);
}


/* Search for duplicate entries in history list, save first one and remove
 * older ones. */
static struct input_history_entry *
check_duplicate_entries(struct input_history *history, unsigned char *data)
{
	struct input_history_entry *entry, *first_duplicate = NULL;

	if (!history || !data || !*data) return NULL;

	foreach (entry, history->entries) {
		struct input_history_entry *duplicate;

		if (strcmp(entry->data, data)) continue;

		/* Found a duplicate -> remove it from history list */

		duplicate = entry;
		entry = entry->prev;

		del_from_history_list(history, duplicate);

		/* Save the first duplicate entry */
		if (!first_duplicate) {
			first_duplicate = duplicate;
		} else {
			mem_free(duplicate);
		}
	}

	return first_duplicate;
}

/* Add a new entry in inputbox history list, take care of duplicate if
 * check_duplicate and respect history size limit. */
void
add_to_input_history(struct input_history *history, unsigned char *data,
		     int check_duplicate)
{
	struct input_history_entry *entry;
	int length;

	if (!history || !data || !*data)
		return;

	/* Strip spaces at the margins */
	trim_chars(data, ' ', &length);
	if (!length) return;

	if (check_duplicate) {
		entry = check_duplicate_entries(history, data);
		if (entry) {
			add_to_history_list(history, entry);
			return;
		}
	}

	/* Copy it all etc. */
	/* One byte is already reserved for url in struct input_history_item. */
	entry = mem_alloc(sizeof(*entry) + length);
	if (!entry) return;

	memcpy(entry->data, data, length + 1);

	add_to_history_list(history, entry);

	/* Limit size of history to MAX_INPUT_HISTORY_ENTRIES, removing first
	 * entries if needed. */
	while (history->size > MAX_INPUT_HISTORY_ENTRIES) {
		if (list_empty(history->entries)) {
			INTERNAL("history is empty");
			history->size = 0;
			return;
		}

		entry = history->entries.prev;
		del_from_history_list(history, entry);
		mem_free(entry);
	}
}

/* Load history file */
int
load_input_history(struct input_history *history, unsigned char *filename)
{
	unsigned char *history_file = filename;
	unsigned char line[MAX_STR_LEN];
	FILE *file;

	if (get_cmd_opt_bool("anonymous")) return 0;
	if (elinks_home) {
		history_file = straconcat(elinks_home, filename,
					  (unsigned char *) NULL);
		if (!history_file) return 0;
	}

	file = fopen(history_file, "rb");
	if (elinks_home) mem_free(history_file);
	if (!file) return 0;

	history->nosave = 1;

	while (fgets(line, MAX_STR_LEN, file)) {
		/* Drop '\n'. */
		if (*line) line[strlen(line) - 1] = 0;
		add_to_input_history(history, line, 0);
	}

	history->nosave = 0;

	fclose(file);

	return 0;
}

/* Write history list to file. It returns a value different from 0 in case of
 * failure, 0 on success. */
int
save_input_history(struct input_history *history, unsigned char *filename)
{
	struct input_history_entry *entry;
	struct secure_save_info *ssi;
	unsigned char *history_file;
	int i = 0;

	if (!history->dirty
	    || !elinks_home
	    || get_cmd_opt_bool("anonymous"))
		return 0;

	history_file = straconcat(elinks_home, filename,
				  (unsigned char *) NULL);
	if (!history_file) return -1;

	ssi = secure_open(history_file);
	mem_free(history_file);
	if (!ssi) return -1;

	foreachback (entry, history->entries) {
		if (i++ > MAX_INPUT_HISTORY_ENTRIES) break;
		secure_fputs(ssi, entry->data);
		secure_fputc(ssi, '\n');
		if (ssi->err) break;
	}

	if (!ssi->err) history->dirty = 0;

	return secure_close(ssi);
}

void
dlg_set_history(struct widget_data *widget_data)
{
	assert(widget_has_history(widget_data));
	assert(widget_data->widget->datalen > 0);

	if ((void *) widget_data->info.field.cur_hist != &widget_data->info.field.history) {
		unsigned char *s = widget_data->info.field.cur_hist->data;

		widget_data->info.field.cpos = int_min(strlen(s), widget_data->widget->datalen - 1);
		if (widget_data->info.field.cpos)
			memcpy(widget_data->cdata, s, widget_data->info.field.cpos);
	} else {
		widget_data->info.field.cpos = 0;
	}

	widget_data->cdata[widget_data->info.field.cpos] = 0;
	widget_data->info.field.vpos = int_max(0, widget_data->info.field.cpos - widget_data->box.width);
}
