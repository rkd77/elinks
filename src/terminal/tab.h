#ifndef EL__TERMINAL_TAB_H
#define EL__TERMINAL_TAB_H

#include "terminal/terminal.h"
#include "terminal/window.h"

struct session;
struct uri;

struct window *
init_tab(struct terminal *term, void *data, window_handler_T handler);

int number_of_tabs(struct terminal *);
int get_tab_number(struct window *);
int get_tab_number_by_xpos(struct terminal *term, int xpos);
struct window *get_tab_by_number(struct terminal *, int);
void switch_to_tab(struct terminal *, int, int);
void switch_current_tab(struct session *ses, int direction);
void close_tab(struct terminal *, struct session *);
void close_all_tabs_but_current(struct session *ses);

#define get_current_tab(term) get_tab_by_number((term), (term)->current_tab)

#define inactive_tab(win) \
	((win)->type != WINDOW_NORMAL && (win) != get_current_tab((win->term)))

void open_uri_in_new_tab(struct session *ses, struct uri *uri, int in_background, int based);
void delayed_open(void *);
void open_current_link_in_new_tab(struct session *ses, int in_background);

void move_current_tab(struct session *ses, int direction);

#define foreach_tab(tab, terminal) \
	foreach (tab, terminal) if (tab->type == WINDOW_TAB)

#define foreachback_tab(tab, terminal) \
	foreachback (tab, terminal) if (tab->type == WINDOW_TAB)

#endif
