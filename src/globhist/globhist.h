#ifndef EL__GLOBHIST_GLOBHIST_H
#define EL__GLOBHIST_GLOBHIST_H

#include "main/object.h"
#include "util/lists.h"
#include "util/time.h"

struct listbox_item;
struct input_history;

struct global_history_item {
	OBJECT_HEAD(struct global_history_item);

	struct listbox_item *box_item;

	unsigned char *title;
	unsigned char *url;

	time_t last_visit;
};

extern struct input_history global_history;

extern unsigned char *gh_last_searched_title;
extern unsigned char *gh_last_searched_url;

extern struct module global_history_module;

void delete_global_history_item(struct global_history_item *);
struct global_history_item *get_global_history_item(unsigned char *);
void add_global_history_item(unsigned char *, unsigned char *, time_t);
int globhist_simple_search(unsigned char *, unsigned char *);

#endif
