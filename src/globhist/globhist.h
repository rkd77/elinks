#ifndef EL__GLOBHIST_GLOBHIST_H
#define EL__GLOBHIST_GLOBHIST_H

#include "main/object.h"
#include "util/lists.h"
#include "util/time.h"

#ifdef __cplusplus
extern "C" {
#endif

struct listbox_item;
struct input_history;

struct global_history_item {
	OBJECT_HEAD(struct global_history_item);

	struct listbox_item *box_item;

	char *title;
	char *url;

	time_t last_visit;
};

extern struct input_history global_history;

extern char *gh_last_searched_title;
extern char *gh_last_searched_url;

extern struct module global_history_module;

void delete_global_history_item(struct global_history_item *);
struct global_history_item *get_global_history_item(char *);
void add_global_history_item(char *, char *, time_t);
int globhist_simple_search(char *, char *);

#ifdef __cplusplus
}
#endif

#endif
