
#ifndef EL__VIEWER_TEXT_FORM_H
#define EL__VIEWER_TEXT_FORM_H

#include "document/forms.h"
#include "util/lists.h" /* LIST_HEAD */
#include "viewer/action.h"

struct document;
struct document_view;
struct link;
struct session;
struct term_event;
struct terminal;

/*! This struct looks a little embarrassing, yeah. */
struct form_view {
	LIST_HEAD(struct form_view);

	/** The corresponding form.form_num within the document.
	 * We can't just reference to struct form since we can potentially
	 * live much longer than that.
	 * @see find_form_by_form_view() */
	int form_num;

#ifdef CONFIG_ECMASCRIPT
	/** This holds the ECMAScript object attached to this structure. It can
	 * be NULL since the object is created on-demand at the first time some
	 * ECMAScript code accesses it. It is freed automatically by the
	 * garbage-collecting code when the ECMAScript context is over (usually
	 * when the document is destroyed). */
	void *ecmascript_obj;
#endif
};

struct form_state {
	struct form_view *form_view;
	int g_ctrl_num;
	int position;
	enum form_type type;

	/* Editable string.
	 * - For ::FC_TEXT, ::FC_PASSWORD, ::FC_FILE, and
	 *   ::FC_TEXTAREA, @c value is the text string that the user
	 *   can edit.  The string is null-terminated; its length is
	 *   not stored separately.  The size of the buffer is not
	 *   stored anywhere; extending the string always requires
	 *   calling realloc().  The string is not normally allowed to
	 *   grow past form_control.maxlength bytes (not counting the
	 *   null), but there may be ways to get longer strings.  The
	 *   string is in the charset of the terminal (which can be
	 *   UTF-8 only if CONFIG_UTF8 is defined, and is assumed to
	 *   be unibyte otherwise).  The charset of the document and
	 *   the UTF-8 I/O option have no effect here.  */
	unsigned char *value;
	/** Position in #value, or an editable integer.
	 * - For ::FC_TEXT, ::FC_PASSWORD, and ::FC_FILE, @c state is
	 *   the byte position of the insertion point in #value.
	 * - For ::FC_CHECKBOX and ::FC_RADIO, @c state is 1 or 0.
	 * - For ::FC_SELECT, @c state is the index of the selected item
	 *   in form_control.labels.  */
	int state;
#ifdef CONFIG_UTF8
	/** Position in the screen.
	 * - For ::FC_TEXT, ::FC_PASSWORD, and ::FC_FILE, @c
	 *   state_cell is not used.  */
	int state_cell;
#endif /* CONFIG_UTF8 */
	/** Horizontal scrolling.
	 * - For ::FC_TEXT, ::FC_PASSWORD, and ::FC_FILE, @c vpos is
	 *   the index of the first displayed byte in #value.  It
	 *   should never be in the middle of a character.  */
	int vpos;
	/** Vertical scrolling.  */
	int vypos;

#ifdef CONFIG_ECMASCRIPT
	/** This holds the ECMAScript object attached to this structure. It can
	 * be NULL since the object is created on-demand at the first time some
	 * ECMAScript code accesses it. It is freed automatically by the
	 * garbage-collecting code when the ECMAScript context is over (usually
	 * when the document is destroyed). */
	void *ecmascript_obj;
#endif
};

struct submitted_value {
	LIST_HEAD(struct submitted_value);

	unsigned char *name;
	unsigned char *value;

	struct form_control *form_control;

	enum form_type type;
	int position;
};

struct submitted_value *init_submitted_value(unsigned char *name, unsigned char *value, enum form_type type, struct form_control *fc, int position);
void done_submitted_value(struct submitted_value *sv);
void done_submitted_value_list(LIST_OF(struct submitted_value) *list);
unsigned char *encode_crlf(struct submitted_value *sv);

struct uri *get_form_uri(struct session *ses, struct document_view *doc_view, struct form_control *fc);

unsigned char *get_form_info(struct session *ses, struct document_view *doc_view);

void selected_item(struct terminal *term, void *item_, void *ses_);
int get_current_state(struct session *ses);

struct form_state *find_form_state(struct document_view *doc_view, struct form_control *fc);
struct form_control *find_form_control(struct document *document, struct form_state *fs);
struct form_view *find_form_view_in_vs(struct view_state *vs, int form_num);
struct form_view *find_form_view(struct document_view *doc_view, struct form *form);
struct form *find_form_by_form_view(struct document *document, struct form_view *fv);

void done_form_state(struct form_state *);
void done_form_view(struct form_view *);

enum frame_event_status field_op(struct session *ses, struct document_view *doc_view, struct link *link, struct term_event *ev);

void draw_form_entry(struct terminal *term, struct document_view *doc_view, struct link *link);
void draw_forms(struct terminal *term, struct document_view *doc_view);

enum frame_event_status reset_form(struct session *ses, struct document_view *doc_view, int a);
enum frame_event_status submit_form(struct session *ses, struct document_view *doc_view, int do_reload);
void submit_given_form(struct session *ses, struct document_view *doc_view, struct form *form, int do_reload);
void auto_submit_form(struct session *ses);
void do_reset_form(struct document_view *doc_view, struct form *form);

void link_form_menu(struct session *ses);

#endif
