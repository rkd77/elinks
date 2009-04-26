/* Global history */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_TIME_H
#include <time.h>
#endif

#include "bfu/dialog.h"
#include "config/home.h"
#include "config/options.h"
#include "globhist/dialogs.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "main/object.h"
#include "main/select.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/hash.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"
#include "util/lists.h"
#include "util/time.h"

#define GLOBAL_HISTORY_FILENAME		"globhist"


INIT_INPUT_HISTORY(global_history);
INIT_LIST_OF(struct global_history_item, global_history_reap_list);


/* GUI stuff. Declared here because done_global_history() frees it. */
unsigned char *gh_last_searched_title = NULL;
unsigned char *gh_last_searched_url = NULL;

enum global_history_options {
	GLOBHIST_TREE,

	GLOBHIST_ENABLE,
	GLOBHIST_MAX_ITEMS,
	GLOBHIST_DISPLAY_TYPE,

	GLOBHIST_OPTIONS,
};

static struct option_info global_history_options[] = {
	INIT_OPT_TREE("document.history", N_("Global history"),
		"global", 0,
		N_("Global history options.")),

	INIT_OPT_BOOL("document.history.global", N_("Enable"),
		"enable", 0, 1,
		N_("Enable global history (\"history of all pages "
		"visited\").")),

	INIT_OPT_INT("document.history.global", N_("Maximum number of entries"),
		"max_items", 0, 1, INT_MAX, 1024,
		N_("Maximum number of entries in the global history.")),

	INIT_OPT_INT("document.history.global", N_("Display style"),
		"display_type", 0, 0, 1, 0,
		N_("What to display in global history dialog:\n"
		"0 is URLs\n"
		"1 is page titles")),

	/* Compatibility alias: added by jonas at 2004-07-16, 0.9.CVS. */
	INIT_OPT_ALIAS("document.history.global", "write_interval", 0,
		"infofiles.save_interval"),

	NULL_OPTION_INFO,
};

#define get_opt_globhist(which)		global_history_options[(which)].option.value
#define get_globhist_enable()		get_opt_globhist(GLOBHIST_ENABLE).number
#define get_globhist_max_items()	get_opt_globhist(GLOBHIST_MAX_ITEMS).number
#define get_globhist_display_type()	get_opt_globhist(GLOBHIST_DISPLAY_TYPE).number

static struct hash *globhist_cache = NULL;
static int globhist_cache_entries = 0;


static void
remove_item_from_global_history(struct global_history_item *history_item)
{
	del_from_history_list(&global_history, history_item);

	if (globhist_cache) {
		struct hash_item *item;

		item = get_hash_item(globhist_cache, history_item->url, strlen(history_item->url));
		if (item) {
			del_hash_item(globhist_cache, item);
			globhist_cache_entries--;
		}
	}
}

static void
reap_deleted_globhist_items(void)
{
	struct global_history_item *history_item, *next;

	foreachsafe(history_item, next, global_history_reap_list) {
		if (!is_object_used(history_item)) {
			del_from_list(history_item);
			mem_free(history_item->title);
			mem_free(history_item->url);
			mem_free(history_item);
		}
	}
}

static void
done_global_history_item(struct global_history_item *history_item)
{
	done_listbox_item(&globhist_browser, history_item->box_item);

	history_item->box_item = NULL;

	add_to_list(global_history_reap_list, history_item);
}

void
delete_global_history_item(struct global_history_item *history_item)
{
	remove_item_from_global_history(history_item);

	done_global_history_item(history_item);
}

/* Search global history for item matching url. */
struct global_history_item *
get_global_history_item(unsigned char *url)
{
	struct hash_item *item;

	if (!url || !globhist_cache) return NULL;

	/* Search for cached entry. */

	item = get_hash_item(globhist_cache, url, strlen(url));

	return item ? (struct global_history_item *) item->value : NULL;
}

#if 0
/* Search global history for certain item. There must be full match with the
 * parameter or the parameter must be NULL/zero. */
struct global_history_item *
multiget_global_history_item(unsigned char *url, unsigned char *title, time_t time)
{
	struct global_history_item *history_item;

	/* Code duplication vs performance, since this function is called most
	 * of time for url matching only... Execution time is divided by 2. */
	if (url && !title && !time) {
		return get_global_history_item(url);
	} else {
		foreach (history_item, global_history.items) {
			if ((!url || !strcmp(history_item->url, url)) &&
			    (!title || !strcmp(history_item->title, title)) &&
			    (!time || history_item->last_visit == time)) {
				return history_item;
			}
		}
	}

	return NULL;
}
#endif

static struct global_history_item *
init_global_history_item(unsigned char *url, unsigned char *title, time_t vtime)
{
	struct global_history_item *history_item;

	history_item = mem_calloc(1, sizeof(*history_item));
	if (!history_item)
		return NULL;

	history_item->last_visit = vtime;
	history_item->title = stracpy(empty_string_or_(title));
	if (!history_item->title) {
		mem_free(history_item);
		return NULL;
	}
	sanitize_title(history_item->title);

	history_item->url = stracpy(url);
	if (!history_item->url || !sanitize_url(history_item->url)) {
		mem_free_if(history_item->url);
		mem_free(history_item->title);
		mem_free(history_item);
		return NULL;
	}

	history_item->box_item = add_listbox_leaf(&globhist_browser, NULL,
						  history_item);
	if (!history_item->box_item) {
		mem_free(history_item->url);
		mem_free(history_item->title);
		mem_free(history_item);
		return NULL;
	}

	object_nolock(history_item, "globhist");

	return history_item;
}

static int
cap_global_history(int max_globhist_items)
{
	while (global_history.size >= max_globhist_items) {
		struct global_history_item *history_item;

		history_item = global_history.entries.prev;

		if ((void *) history_item == &global_history.entries) {
			INTERNAL("global history is empty");
			global_history.size = 0;
			return 0;
		}

		delete_global_history_item(history_item);
	}

	return 1;
}

static void
add_item_to_global_history(struct global_history_item *history_item,
			   int max_globhist_items)
{
	add_to_history_list(&global_history, history_item);

	/* Hash creation if needed. */
	if (!globhist_cache)
		globhist_cache = init_hash8();

	if (globhist_cache && globhist_cache_entries < max_globhist_items) {
		int urllen = strlen(history_item->url);

		/* Create a new entry. */
		if (add_hash_item(globhist_cache, history_item->url, urllen, history_item)) {
			globhist_cache_entries++;
		}
	}
}

/* Add a new entry in history list, take care of duplicate, respect history
 * size limit, and update any open history dialogs. */
void
add_global_history_item(unsigned char *url, unsigned char *title, time_t vtime)
{
	struct global_history_item *history_item;
	int max_globhist_items;

	if (!url || !get_globhist_enable()) return;

	max_globhist_items = get_globhist_max_items();

	history_item = get_global_history_item(url);
	if (history_item) delete_global_history_item(history_item);

	if (!cap_global_history(max_globhist_items)) return;

	reap_deleted_globhist_items();

	history_item = init_global_history_item(url, title, vtime);
	if (!history_item) return;

	add_item_to_global_history(history_item, max_globhist_items);
}


int
globhist_simple_search(unsigned char *search_url, unsigned char *search_title)
{
	struct global_history_item *history_item;

	if (!search_title || !search_url)
		return 0;

	/* Memorize last searched title */
	mem_free_set(&gh_last_searched_title, stracpy(search_title));
	if (!gh_last_searched_title) return 0;

	/* Memorize last searched url */
	mem_free_set(&gh_last_searched_url, stracpy(search_url));
	if (!gh_last_searched_url) return 0;

	if (!*search_title && !*search_url) {
		/* No search terms, make all entries visible. */
		foreach (history_item, global_history.entries) {
			history_item->box_item->visible = 1;
		}
		return 1;
	}

	foreach (history_item, global_history.entries) {
		/* Make matching entries visible, hide others. */
		if ((*search_title
		     && strcasestr(history_item->title, search_title))
		    || (*search_url
			&& c_strcasestr(history_item->url, search_url))) {
			history_item->box_item->visible = 1;
		} else {
			history_item->box_item->visible = 0;
		}
	}
	return 1;
}


static void
read_global_history(void)
{
	unsigned char in_buffer[MAX_STR_LEN * 3];
	unsigned char *file_name = GLOBAL_HISTORY_FILENAME;
	unsigned char *title;
	FILE *f;

	if (!get_globhist_enable()
	    || get_cmd_opt_bool("anonymous"))
		return;

	if (elinks_home) {
		file_name = straconcat(elinks_home, file_name,
				       (unsigned char *) NULL);
		if (!file_name) return;
	}
	f = fopen(file_name, "rb");
	if (elinks_home) mem_free(file_name);
	if (!f) return;

	title = in_buffer;
	global_history.nosave = 1;

	while (fgets(in_buffer, sizeof(in_buffer), f)) {
		unsigned char *url, *last_visit, *eol;

		url = strchr(title, '\t');
		if (!url) continue;
		*url++ = '\0'; /* Now url points to the character after \t. */

		last_visit = strchr(url, '\t');
		if (!last_visit) continue;
		*last_visit++ = '\0';

		eol = strchr(last_visit, '\n');
		if (!eol) continue;
		*eol = '\0'; /* Drop ending '\n'. */

		add_global_history_item(url, title, str_to_time_t(last_visit));
	}

	global_history.nosave = 0;
	fclose(f);
}

static void
write_global_history(void)
{
	struct global_history_item *history_item;
	unsigned char *file_name;
	struct secure_save_info *ssi;

	if (!global_history.dirty || !elinks_home
	    || !get_globhist_enable()
	    || get_cmd_opt_bool("anonymous"))
		return;

	file_name = straconcat(elinks_home, GLOBAL_HISTORY_FILENAME,
			       (unsigned char *) NULL);
	if (!file_name) return;

	ssi = secure_open(file_name);
	mem_free(file_name);
	if (!ssi) return;

	foreachback (history_item, global_history.entries) {
		if (secure_fprintf(ssi, "%s\t%s\t%"TIME_PRINT_FORMAT"\n",
				   history_item->title,
				   history_item->url,
				   (time_print_T) history_item->last_visit) < 0)
			break;
	}

	if (!secure_close(ssi)) global_history.dirty = 0;
}

static void
free_global_history(void)
{
	if (globhist_cache) {
		free_hash(&globhist_cache);
		globhist_cache_entries = 0;
	}

	while (!list_empty(global_history.entries))
		delete_global_history_item(global_history.entries.next);

	reap_deleted_globhist_items();
}

static enum evhook_status
global_history_write_hook(va_list ap, void *data)
{
	write_global_history();
	return EVENT_HOOK_STATUS_NEXT;
}

struct event_hook_info global_history_hooks[] = {
	{ "periodic-saving", 0, global_history_write_hook, NULL },

	NULL_EVENT_HOOK_INFO,
};

static void
init_global_history(struct module *module)
{
	read_global_history();
}

static void
done_global_history(struct module *module)
{
	write_global_history();
	free_global_history();
	mem_free_if(gh_last_searched_title);
	mem_free_if(gh_last_searched_url);
}

struct module global_history_module = struct_module(
	/* name: */		N_("Global History"),
	/* options: */		global_history_options,
	/* events: */		global_history_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_global_history,
	/* done: */		done_global_history
);
