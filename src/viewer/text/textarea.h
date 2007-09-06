
#ifndef EL__VIEWER_TEXT_TEXTAREA_H
#define EL__VIEWER_TEXT_TEXTAREA_H

/* This file is largely a supserset of this header, so it doesn't hurt to just
 * include it here, IMHO. --pasky */
#include "viewer/action.h"
#include "viewer/text/form.h"

struct document_view;
struct form_control;
struct link;
struct session;
struct terminal;

#ifdef CONFIG_UTF8
int area_cursor(struct form_control *fc, struct form_state *fs, int utf8);
#else
int area_cursor(struct form_control *fc, struct form_state *fs);
#endif /* CONFIG_UTF8 */
void draw_textarea(struct terminal *term, struct form_state *fs, struct document_view *doc_view, struct link *link);
unsigned char *encode_textarea(struct submitted_value *sv);

void free_textarea_data(struct terminal *term);
void textarea_edit(int, struct terminal *, struct form_state *, struct document_view *, struct link *);
void menu_textarea_edit(struct terminal *term, void *xxx, void *ses_);

#ifdef CONFIG_UTF8
enum frame_event_status textarea_op_home(struct form_state *fs, struct form_control *fc, int utf8);
enum frame_event_status textarea_op_up(struct form_state *fs, struct form_control *fc, int utf8);
enum frame_event_status textarea_op_down(struct form_state *fs, struct form_control *fc, int utf8);
enum frame_event_status textarea_op_end(struct form_state *fs, struct form_control *fc, int utf8);
enum frame_event_status textarea_op_bob(struct form_state *fs, struct form_control *fc, int utf8);
enum frame_event_status textarea_op_eob(struct form_state *fs, struct form_control *fc, int utf8);
enum frame_event_status textarea_op_enter(struct form_state *fs, struct form_control *fc, int utf8);
enum frame_event_status textarea_op_left(struct form_state *fs, struct form_control *fc, int utf8);
enum frame_event_status textarea_op_right(struct form_state *fs, struct form_control *fc, int utf8);
#else
enum frame_event_status textarea_op_home(struct form_state *fs, struct form_control *fc);
enum frame_event_status textarea_op_up(struct form_state *fs, struct form_control *fc);
enum frame_event_status textarea_op_down(struct form_state *fs, struct form_control *fc);
enum frame_event_status textarea_op_end(struct form_state *fs, struct form_control *fc);
enum frame_event_status textarea_op_bob(struct form_state *fs, struct form_control *fc);
enum frame_event_status textarea_op_eob(struct form_state *fs, struct form_control *fc);
enum frame_event_status textarea_op_enter(struct form_state *fs, struct form_control *fc);
#endif /* CONFIG_UTF8 */

void set_textarea(struct document_view *doc_view, int direction);

#endif
