#ifndef EL__DIALOGS_MENU_H
#define EL__DIALOGS_MENU_H

#include "bfu/menu.h"
#include "cache/cache.h"
#include "session/task.h"
#include "terminal/terminal.h"
#include "viewer/action.h"

struct document_view;
struct open_in_new;
struct session;
struct uri;

void activate_bfu_technology(struct session *, int);

void dialog_goto_url(struct session *ses, unsigned char *url);
/* void dialog_save_url(struct session *ses); */

void tab_menu(struct session *ses, int x, int y,
	      int place_above_cursor);

void free_history_lists(void);

void query_file(struct session *, struct uri *, void *, void (*)(void *, unsigned char *), void (*)(void *), int);

void query_exit(struct session *ses);
void exit_prog(struct session *ses, int query);

void save_url_as(struct session *ses);

void
open_uri_in_new_window(struct session *ses, struct uri *uri, struct uri *referrer,
		       enum term_env_type env, enum cache_mode cache_mode,
		       enum task_type task);

void send_open_new_window(struct terminal *term, const struct open_in_new *open, struct session *ses);
void send_open_in_new_window(struct terminal *term, const struct open_in_new *open, struct session *ses);

void open_in_new_window(struct terminal *term, void *func_, void *ses_);

void add_new_win_to_menu(struct menu_item **mi, unsigned char *text, struct terminal *term);

/* URI passing: */

/* Controls what URI to use */
enum pass_uri_type {
	PASS_URI_FRAME,
	PASS_URI_LINK,
	PASS_URI_TAB,
};

void add_uri_command_to_menu(struct menu_item **mi, enum pass_uri_type type, unsigned char *text);
enum frame_event_status pass_uri_to_command(struct session *ses, struct document_view *doc_view, int /* enum pass_uri_type */ type);

void
auto_complete_file(struct terminal *term, int no_elevator, unsigned char *path,
		   menu_func_T file_func, menu_func_T dir_func, void *data);

#endif
