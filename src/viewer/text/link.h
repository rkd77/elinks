/* $Id: link.h,v 1.47 2004/11/22 13:27:42 zas Exp $ */

#ifndef EL__VIEWER_TEXT_LINK_H
#define EL__VIEWER_TEXT_LINK_H

struct document;
struct document_view;
struct link;
struct session;
struct term_event;
struct terminal;
struct uri;

void set_link(struct document_view *doc_view);
void free_link(struct document_view *doc_view);
void clear_link(struct terminal *term, struct document_view *doc_view);
void draw_current_link(struct session *ses, struct document_view *doc_view);

void link_menu(struct terminal *term, void *, void *ses);

struct link *get_first_link(struct document_view *doc_view);
struct link *get_last_link(struct document_view *doc_view);
struct link *get_link_at_coordinates(struct document_view *doc_view,
				     int x, int y);

unsigned char *get_current_link_title(struct document_view *doc_view);
unsigned char *get_current_link_info(struct session *ses, struct document_view *doc_view);

void set_pos_x(struct document_view *doc_view, struct link *link);
void set_pos_y(struct document_view *doc_view, struct link *link);

void find_link_up(struct document_view *doc_view);
void find_link_page_up(struct document_view *doc_view);
void find_link_down(struct document_view *doc_view);
void find_link_page_down(struct document_view *doc_view);

int current_link_is_visible(struct document_view *doc_view);
int link_in_view(struct document_view *doc_view, struct link *link);
int link_in_view_y(struct document_view *doc_view, struct link *link);
int next_link_in_view(struct document_view *doc_view, int p, int d, int (*fn)(struct document_view *, struct link *), void (*cntr)(struct document_view *, struct link *));
int next_link_in_dir(struct document_view *doc_view, int dir_x, int dir_y);

void jump_to_link_number(struct session *ses, struct document_view *doc_view, int);

struct link *goto_current_link(struct session *ses, struct document_view *, int);
void goto_link_number(struct session *ses, unsigned char *num);

/* Bruteforce compilation fixes */
enum frame_event_status enter(struct session *ses, struct document_view *doc_view, int do_reload);
enum frame_event_status try_document_key(struct session *ses,
					 struct document_view *doc_view,
					 struct term_event *ev);

struct uri *get_link_uri(struct session *ses, struct document_view *doc_view, struct link *link);

#endif
