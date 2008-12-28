/** Forms viewing/manipulation handling
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we want memrchr() ! */
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif

#include "elinks.h"

#include "bfu/listmenu.h"
#include "bfu/dialog.h"
#include "config/kbdbind.h"
#include "dialogs/menu.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/html/parser.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "intl/gettext/libintl.h"
#include "formhist/formhist.h"
#include "mime/mime.h"
#include "osdep/ascii.h"
#include "osdep/osdep.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/action.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/textarea.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


/* TODO: Some of these (particulary those encoding routines) would feel better
 * in viewer/common/. --pasky */

/** @relates submitted_value */
struct submitted_value *
init_submitted_value(unsigned char *name, unsigned char *value, enum form_type type,
		     struct form_control *fc, int position)
{
	struct submitted_value *sv;

	sv = mem_alloc(sizeof(*sv));
	if (!sv) return NULL;

	sv->value = stracpy(value);
	if (!sv->value) { mem_free(sv); return NULL; }

	sv->name = stracpy(name);
	if (!sv->name) { mem_free(sv->value); mem_free(sv); return NULL; }

	sv->type = type;
	sv->form_control = fc;
	sv->position = position;

	return sv;
}

/** @relates submitted_value */
void
done_submitted_value(struct submitted_value *sv)
{
	if (!sv) return;
	mem_free_if(sv->value);
	mem_free_if(sv->name);
	mem_free(sv);
}

static void
fixup_select_state(struct form_control *fc, struct form_state *fs)
{
	int i;

	assert(fc && fs);
	if_assert_failed return;

	if (fs->state >= 0
	    && fs->state < fc->nvalues
	    && !strcmp(fc->values[fs->state], fs->value))
		return;

	for (i = 0; i < fc->nvalues; i++)
		if (!strcmp(fc->values[i], fs->value)) {
			fs->state = i;
			return;
		}

	fs->state = 0;

	mem_free_set(&fs->value, stracpy(fc->nvalues
					 ? fc->values[0]
					 : (unsigned char *) ""));
}

/* menu_func_T */
void
selected_item(struct terminal *term, void *item_, void *ses_)
{
	struct session *ses = ses_;
	int item = (long) item_;
	struct document_view *doc_view;
	struct link *link;
	struct form_state *fs;
	struct form_control *fc;

	assert(term && ses);
	if_assert_failed return;
	doc_view = current_frame(ses);

	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	link = get_current_link(doc_view);
	if (!link || link->type != LINK_SELECT) return;

	fc = get_link_form_control(link);
	fs = find_form_state(doc_view, fc);
	if (fs) {
		if (item >= 0 && item < fc->nvalues) {
			fs->state = item;
			mem_free_set(&fs->value, stracpy(fc->values[item]));
		}
		fixup_select_state(fc, fs);
	}

	refresh_view(ses, doc_view, 0);
}

static void
init_form_state(struct document_view *doc_view,
		struct form_control *fc, struct form_state *fs)
{
	struct terminal *term;
	int doc_cp, viewer_cp;

	assert(fc && fs);
	if_assert_failed return;

	doc_cp = doc_view->document->cp;
	term = doc_view->session->tab->term;
	viewer_cp = get_terminal_codepage(term);

	mem_free_set(&fs->value, NULL);

	switch (fc->type) {
		case FC_TEXT:
		case FC_PASSWORD:
#ifdef CONFIG_FORMHIST
			fs->value = null_or_stracpy(
				get_form_history_value(
					fc->form->action, fc->name));
#endif /* CONFIG_FORMHIST */
			/* fall through */
		case FC_TEXTAREA:
			if (fs->value == NULL) {
				fs->value = convert_string(
					get_translation_table(doc_cp, viewer_cp),
					fc->default_value,
					strlen(fc->default_value),
					viewer_cp, CSM_FORM,
					&fs->state, NULL, NULL);
			}
			fs->state = fs->value ? strlen(fs->value) : 0;
#ifdef CONFIG_UTF8
			if (fc->type == FC_TEXTAREA)
				fs->state_cell = 0;
#endif /* CONFIG_UTF8 */
			fs->vpos = 0;
			break;
		case FC_FILE:
			fs->value = stracpy("");
			fs->state = 0;
			fs->vpos = 0;
			break;
		case FC_SELECT:
			fs->value = convert_string(
				get_translation_table(doc_cp, viewer_cp),
				fc->default_value,
				strlen(fc->default_value),
				viewer_cp, CSM_FORM,
				&fs->state, NULL, NULL);
			fs->state = fc->default_state;
			fixup_select_state(fc, fs);
			break;
		case FC_CHECKBOX:
		case FC_RADIO:
			fs->state = fc->default_state;
			/* Fall-through */
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_BUTTON:
		case FC_HIDDEN:
			/* We don't want to recode hidden fields. */
			fs->value = stracpy(fc->default_value);
			break;
	}
}


struct form_state *
find_form_state(struct document_view *doc_view, struct form_control *fc)
{
	struct view_state *vs;
	struct form_state *fs;
	int n;

	assert(doc_view && doc_view->vs && fc);
	if_assert_failed return NULL;

	vs = doc_view->vs;
	n = fc->g_ctrl_num;

	if (n >= vs->form_info_len) {
		int nn = n + 1;
#ifdef CONFIG_ECMASCRIPT
		const struct form_state *const old_form_info = vs->form_info;
#endif

		fs = mem_align_alloc(&vs->form_info, vs->form_info_len, nn, 0);
		if (!fs) return NULL;
		vs->form_info = fs;
		vs->form_info_len = nn;

#ifdef CONFIG_ECMASCRIPT
		/* TODO: Standard C does not allow this comparison;
		 * if the memory to which old_form_info pointed has
		 * been freed, then the value of the pointer itself is
		 * indeterminate.  Fixing this would require changing
		 * mem_align_alloc to tell the caller whether it did
		 * realloc or not.  */
		if (vs->form_info != old_form_info) {
			/* vs->form_info[] was moved to a different address.
			 * Update all the ECMAScript objects that have
			 * pointers to its elements.  */
			for (nn = 0; nn < vs->form_info_len; nn++)
				ecmascript_moved_form_state(&vs->form_info[nn]);
		}
#endif /* CONFIG_ECMASCRIPT */
	}
	fs = &vs->form_info[n];

	if (fs->form_view && fs->form_view->form_num == fc->form->form_num
	    && fs->g_ctrl_num == fc->g_ctrl_num
	    && fs->position == fc->position
	    && fs->type == fc->type)
		return fs;

	mem_free_if(fs->value);
	memset(fs, 0, sizeof(*fs));
	fs->form_view = find_form_view(doc_view, fc->form);
	fs->g_ctrl_num = fc->g_ctrl_num;
	fs->position = fc->position;
	fs->type = fc->type;
	init_form_state(doc_view, fc, fs);

	return fs;
}

struct form_control *
find_form_control(struct document *document, struct form_state *fs)
{
	struct form *form = find_form_by_form_view(document, fs->form_view);
	struct form_control *fc;

	foreach (fc, form->items) {
		if (fs->g_ctrl_num == fc->g_ctrl_num
		    && fs->position == fc->position
		    && fs->type == fc->type)
			return fc;
	}

	return NULL;
}

struct form_view *
find_form_view_in_vs(struct view_state *vs, int form_num)
{
	struct form_view *fv;

	assert(vs);

	foreach (fv, vs->forms)
		if (fv->form_num == form_num)
			return fv;

	fv = mem_calloc(1, sizeof(*fv));
	fv->form_num = form_num;
	add_to_list(vs->forms, fv);
	return fv;
}

struct form_view *
find_form_view(struct document_view *doc_view, struct form *form)
{
	return find_form_view_in_vs(doc_view->vs, form->form_num);
}

struct form *
find_form_by_form_view(struct document *document, struct form_view *fv)
{
	struct form *form;

	foreach (form, document->forms) {
		if (form->form_num == fv->form_num)
			return form;
	}
	return NULL;
}

/** Free any data owned by @a fs, but not the struct form_state
 * itself, because that is normally allocated as part of an array.
 * @relates form_state */
void
done_form_state(struct form_state *fs)
{
#ifdef CONFIG_ECMASCRIPT
	ecmascript_detach_form_state(fs);
#endif
	mem_free_if(fs->value);
}

/** Free @a fv and any data owned by it.  This does not call
 * del_from_list(fv), so the caller must usually do that first.
 * @relates form_view */
void
done_form_view(struct form_view *fv)
{
#ifdef CONFIG_ECMASCRIPT
	ecmascript_detach_form_view(fv);
#endif
	mem_free(fv);
}

int
get_current_state(struct session *ses)
{
	struct document_view *doc_view;
	struct link *link;
	struct form_state *fs;

	assert(ses);
	if_assert_failed return -1;
	doc_view = current_frame(ses);

	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return -1;

	link = get_current_link(doc_view);
	if (!link || link->type != LINK_SELECT) return -1;

	fs = find_form_state(doc_view, get_link_form_control(link));
	if (fs) return fs->state;
	return -1;
}

void
draw_form_entry(struct terminal *term, struct document_view *doc_view,
		struct link *link)
{
	struct form_state *fs;
	struct form_control *fc;
	struct view_state *vs;
	struct box *box;
	int dx, dy;

	assert(term && doc_view && doc_view->document && doc_view->vs && link);
	if_assert_failed return;

	fc = get_link_form_control(link);
	assertm(fc != NULL, "link %d has no form control", (int) (link - doc_view->document->links));
	if_assert_failed return;

	fs = find_form_state(doc_view, fc);
	if (!fs) return;

	box = &doc_view->box;
	vs = doc_view->vs;
	dx = box->x - vs->x;
	dy = box->y - vs->y;
	switch (fc->type) {
		unsigned char *s;
#ifdef CONFIG_UTF8
		unsigned char *text, *end, *last_in_view;
		int retried;
#endif /* CONFIG_UTF8 */
		int len;
		int i, x, y;

		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
			if (!link->npoints) break;

			y = link->points[0].y + dy;
			if (!row_is_in_box(box, y))
				break;

			x = link->points[0].x + dx;
#ifdef CONFIG_UTF8
			if (term->utf8_cp) goto utf8;
#endif /* CONFIG_UTF8 */
			int_bounds(&fs->vpos, fs->state - fc->size + 1, fs->state);
			len = strlen(fs->value) - fs->vpos;

			for (i = 0; i < fc->size; i++, x++) {
				unsigned char data;

				if (!col_is_in_box(box, x)) continue;

				if (fs->value && i >= -fs->vpos && i < len)
					data = fc->type != FC_PASSWORD
					     ? fs->value[i + fs->vpos] : '*';
				else
					data = '_';

				draw_char_data(term, x, y, data);
			}
			break;
#ifdef CONFIG_UTF8
utf8:
			retried = 0;

retry_after_scroll:
			text = fs->value;
			if (!text) text = "";
			len = strlen(text);
			int_bounds(&fs->state, 0, len);
			int_bounds(&fs->vpos, 0, fs->state);
			end = text + len;
			text += fs->vpos;
			last_in_view = NULL;

			for (i = 0; i < fc->size; ) {
				unicode_val_T data;
				int cells, cell;
				unsigned char *maybe_in_view = text;

				data = utf8_to_unicode(&text, end);
				if (data == UCS_NO_CHAR) /* end of string */
					data = '_';
				else if (fc->type == FC_PASSWORD)
					data = '*';

				cells = unicode_to_cell(data);
				if (i + cells <= fc->size) {
					last_in_view = maybe_in_view;
					if (colspan_is_in_box(box, x + i, cells)) {
						/* The character fits completely.
						 * Draw the character, and mark any
						 * further cells with UCS_NO_CHAR.  */
						draw_char_data(term, x + i, y, data);
						for (cell = 1; cell < cells; cell++)
							draw_char_data(term, x + i + cell,
								       y, UCS_NO_CHAR);
						goto drew_char;
					}
				}

				/* The character does not fit completely.
				 * Write UCS_ORPHAN_CELL to the cells that
				 * do fit.  */
				for (cell = 0; cell < cells; cell++) {
					if (col_is_in_box(box, x + i + cell)
					    && i + cell < fc->size)
						draw_char_data(term,
							       x + i + cell, y,
							       UCS_ORPHAN_CELL);
				}

drew_char:
				i += cells;
			}

			/* The int_bounds calls above ensured that the
			 * insertion point cannot be at the left side
			 * of the scrolled-visible part of the text.
			 * However it can still be at the right side.
			 * Check whether we need to change fs->vpos.
			 *
			 * This implementation attempts to follow
			 * these rules:
			 * - If the insertion point is at the end of
			 *   the string, leave at least one empty cell
			 *   so that there is a place for the cursor.
			 * - If a character follows the insertion
			 *   point, make that character fully visible;
			 *   note the character may be double-width.
			 * - If fc->size < 2, it is not possible to
			 *   make a double-width character fully
			 *   visible.  In this case, it is OK if the
			 *   output is ugly, but ELinks must not fall
			 *   into an infinite loop or crash.
			 * - The length of the string should not affect
			 *   how long this function takes.  The width
			 *   of the widget naturally will.
			 * - Optimize the case where fields are drawn
			 *   several times without being modified.
			 *
			 * It follows that:
			 * - If the "for i" loop above hit UCS_NO_CHAR,
			 *   then there is no need to scroll.
			 * - When the string ends with a double-width
			 *   character that fits in only partially,
			 *   then text==end, but the field may have
			 *   to be scrolled.  */
			if (fs->value && last_in_view
			    && last_in_view < fs->value + fs->state) {
				unsigned char *ptr = fs->value + fs->state;
				int cells = fc->size;
				enum utf8_step how = (fc->type == FC_PASSWORD)
					? UTF8_STEP_CHARACTERS
					: UTF8_STEP_CELLS_FEWER;

				/* The insertion point is at the right
				 * side of the scrolled-visible part
				 * of the text.  Decide a new fs->vpos
				 * by counting cells backwards from
				 * @ptr.  But first advance @ptr past
				 * the character that follows the
				 * insertion point, so that it will be
				 * fully displayed.  If there is no
				 * such character, reserve one cell
				 * for the cursor anyway.  */
				if (utf8_to_unicode(&ptr, end) == UCS_NO_CHAR)
					--cells;
				ptr = utf8_step_backward(ptr, fs->value,
							 cells, how, NULL);

				if (fs->vpos != ptr - fs->value) {
					fs->vpos = ptr - fs->value;
					retried = 1;
					goto retry_after_scroll;
				}
			}
			break;
#endif /* CONFIG_UTF8 */
		case FC_TEXTAREA:
			draw_textarea(term, fs, doc_view, link);
			break;
		case FC_CHECKBOX:
		case FC_RADIO:
			if (link->npoints < 2) break;
			x = link->points[1].x + dx;
			y = link->points[1].y + dy;
			if (is_in_box(box, x, y))
				draw_char_data(term, x, y, fs->state ? 'X' : ' ');
			break;
		case FC_SELECT:
			fixup_select_state(fc, fs);
			if (fs->state < fc->nvalues)
				s = fc->labels[fs->state];
			else
				/* XXX: when can this happen? --pasky */
				s = "";
#ifdef CONFIG_UTF8
			if (term->utf8_cp) goto utf8_select;
#endif /* CONFIG_UTF8 */
			len = s ? strlen(s) : 0;
			for (i = 0; i < link->npoints; i++) {
				x = link->points[i].x + dx;
				y = link->points[i].y + dy;
				if (is_in_box(box, x, y))
					draw_char_data(term, x, y, i < len ? s[i] : '_');
			}
			break;
#ifdef CONFIG_UTF8
utf8_select:
			text = s;
			end = strchr(s, '\0');
			len = utf8_ptr2cells(text, end);
			for (i = 0; i < link->npoints; i++) {
				x = link->points[i].x + dx;
				y = link->points[i].y + dy;
				if (is_in_box(box, x, y)) {
					unicode_val_T data;
					if (i < len) {
						int cell;

						data = utf8_to_unicode(&s, end);
						cell = unicode_to_cell(data);
						if (i + 1 < len && cell == 2) {
							draw_char_data(term, x++, y, data);

							data = UCS_NO_CHAR;
							i++;
						} else if (cell == 2) {
							data = UCS_ORPHAN_CELL;
						}
					} else
						data = '_';
					draw_char_data(term, x, y, data);
				}
			}
			break;
#endif /* CONFIG_UTF8 */
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_BUTTON:
		case FC_HIDDEN:
			break;
	}
}

void
draw_forms(struct terminal *term, struct document_view *doc_view)
{
	struct link *l1, *l2;

	assert(term && doc_view);
	if_assert_failed return;

	l1 = get_first_link(doc_view);
	l2 = get_last_link(doc_view);

	if (!l1 || !l2) {
		assertm(!l1 && !l2, "get_first_link == %p, get_last_link == %p", l1, l2);
		/* Return path :-). */
		return;
	}
	do {
		struct form_control *fc = get_link_form_control(l1);

		if (!fc) continue;
		draw_form_entry(term, doc_view, l1);

	} while (l1++ < l2);
}


/** @relates submitted_value */
void
done_submitted_value_list(LIST_OF(struct submitted_value) *list)
{
	struct submitted_value *sv, *svtmp;

	assert(list);
	if_assert_failed return;

	foreach (sv, *list) {
		svtmp = sv;
		sv = sv->prev;
		del_from_list(svtmp);
		done_submitted_value(svtmp);
	}
}

static void
add_submitted_value_to_list(struct form_control *fc,
		            struct form_state *fs,
		            LIST_OF(struct submitted_value) *list)
{
	struct submitted_value *sub;
	unsigned char *name;
	enum form_type type;
	int position;

	assert(fc && fs && list);

	name = fc->name;
	position = fc->position;
	type = fc->type;

	switch (fc->type) {
	case FC_TEXT:
	case FC_PASSWORD:
	case FC_FILE:
	case FC_TEXTAREA:
		sub = init_submitted_value(name, fs->value, type, fc, position);
		if (sub) add_to_list(*list, sub);
		break;

	case FC_CHECKBOX:
	case FC_RADIO:
		if (!fs->state) break;
		/* fall through */

	case FC_SUBMIT:
	case FC_HIDDEN:
	case FC_RESET:
	case FC_BUTTON:
		sub = init_submitted_value(name, fs->value, type, fc,
					   position);
		if (sub) add_to_list(*list, sub);
		break;

	case FC_SELECT:
		if (!fc->nvalues) break;

		fixup_select_state(fc, fs);
		sub = init_submitted_value(name, fs->value, type, fc, position);
		if (sub) add_to_list(*list, sub);
		break;

	case FC_IMAGE:
	        name = straconcat(fc->name, ".x", (unsigned char *) NULL);
		if (!name) break;
		sub = init_submitted_value(name, "0", type, fc, position);
		mem_free(name);
		if (sub) add_to_list(*list, sub);

		name = straconcat(fc->name, ".y", (unsigned char *) NULL);
		if (!name) break;
		sub = init_submitted_value(name, "0", type, fc, position);
		mem_free(name);
		if (sub) add_to_list(*list, sub);

		break;
	}
}

static void
sort_submitted_values(LIST_OF(struct submitted_value) *list)
{
	while (1) {
		struct submitted_value *sub;
		int changed = 0;

		foreach (sub, *list) if (list_has_next(*list, sub))
			if (sub->next->position < sub->position) {
				struct submitted_value *next = sub->next;

				del_from_list(sub);
				add_at_pos(next, sub);
				sub = next;
				changed = 1;
			}

		foreachback (sub, *list) if (list_has_next(*list, sub))
			if (sub->next->position < sub->position) {
				struct submitted_value *next = sub->next;

				del_from_list(sub);
				add_at_pos(next, sub);
				sub = next;
				changed = 1;
			}

		if (!changed) break;
	};
}

static void
get_successful_controls(struct document_view *doc_view,
			struct form_control *fc,
			LIST_OF(struct submitted_value) *list)
{
	struct form_control *fc2;

	assert(doc_view && fc && fc->form && list);
	if_assert_failed return;

	foreach (fc2, fc->form->items) {
		if (((fc2->type != FC_SUBMIT &&
			 fc2->type != FC_IMAGE &&
			 fc2->type != FC_RESET &&
			 fc2->type != FC_BUTTON) || fc2 == fc)
		    && fc2->name && fc2->name[0]) {
			struct form_state *fs = find_form_state(doc_view, fc2);

			if (!fs) continue;

			add_submitted_value_to_list(fc2, fs, list);
		}
	}

	sort_submitted_values(list);
}

unsigned char *
encode_crlf(struct submitted_value *sv)
{
	struct string newtext;
	int i;

	assert(sv && sv->value);
	if_assert_failed return NULL;

	if (!init_string(&newtext)) return NULL;

	for (i = 0; sv->value[i]; i++) {
		if (sv->value[i] == '\r') {
			if (sv->value[i+1] != '\n')
				add_crlf_to_string(&newtext);
		} else if (sv->value[i] == '\n')
			add_crlf_to_string(&newtext);
		else
			add_char_to_string(&newtext, sv->value[i]);
	}

	return newtext.source;
}

static void
encode_controls(LIST_OF(struct submitted_value) *l, struct string *data,
		int cp_from, int cp_to)
{
	struct submitted_value *sv;
	struct conv_table *convert_table = NULL;
	int lst = 0;

	assert(l && data);
	if_assert_failed return;

	foreach (sv, *l) {
		unsigned char *p2 = NULL;

		if (lst)
			add_char_to_string(data, '&');
		else
			lst = 1;

		encode_uri_string(data, sv->name, strlen(sv->name), 1);
		add_char_to_string(data, '=');

		/* Convert back to original encoding (see html_form_control()
		 * for the original recoding). */
		if (sv->type == FC_TEXTAREA) {
			unsigned char *p;

			p = encode_textarea(sv);
			if (p) {
				if (!convert_table)
					convert_table = get_translation_table(cp_from, cp_to);

				p2 = convert_string(convert_table, p,
						    strlen(p), -1, CSM_FORM, NULL, NULL, NULL);
				mem_free(p);
			}
		} else if (sv->type == FC_TEXT ||
			   sv->type == FC_PASSWORD) {
			if (!convert_table)
				convert_table = get_translation_table(cp_from, cp_to);

			p2 = convert_string(convert_table, sv->value,
					    strlen(sv->value), -1, CSM_FORM, NULL, NULL, NULL);
		} else if (sv->type == FC_HIDDEN) {
			p2 = encode_crlf(sv);
		} else {
			p2 = stracpy(sv->value);
		}

		if (p2) {
			encode_uri_string(data, p2, strlen(p2), 1);
			mem_free(p2);
		}
	}
}



#define BOUNDARY_LENGTH	32
#define realloc_bound_ptrs(bptrs, bptrs_size) \
	mem_align_alloc(bptrs, bptrs_size, bptrs_size + 1, 0xFF)

struct boundary_info {
	int count;
	int *offsets;
	unsigned char string[BOUNDARY_LENGTH];
};

/** @relates boundary_info */
static inline void
init_boundary(struct boundary_info *boundary)
{
	memset(boundary, 0, sizeof(*boundary));
	memset(boundary->string, '0', BOUNDARY_LENGTH);
}

/** Add boundary to string and save the offset
 * @relates boundary_info */
static inline void
add_boundary(struct string *data, struct boundary_info *boundary)
{
	add_to_string(data, "--");

	if (realloc_bound_ptrs(&boundary->offsets, boundary->count))
		boundary->offsets[boundary->count++] = data->length;

	add_bytes_to_string(data, boundary->string, BOUNDARY_LENGTH);
}

/** @relates boundary_info */
static inline unsigned char *
increment_boundary_counter(struct boundary_info *boundary)
{
	int j;

	/* This is just a decimal string incrementation */
	for (j = BOUNDARY_LENGTH - 1; j >= 0; j--) {
		if (boundary->string[j]++ < '9')
			return boundary->string;

		boundary->string[j] = '0';
	}

	INTERNAL("Form data boundary counter overflow");

	return NULL;
}

/** @relates boundary_info */
static inline void
check_boundary(struct string *data, struct boundary_info *boundary)
{
	unsigned char *bound = boundary->string;
	int i;

	/* Search between all boundaries. There is a starting and an ending
	 * boundary so only check the range of chars after the current offset
	 * and before the next offset. If some string in the form data matches
	 * the boundary string it is changed. */
	for (i = 0; i < boundary->count - 1; i++) {
		/* Start after the boundary string and also jump past the
		 * "\r\nContent-Disposition: form-data; name=\"" string added
		 * before any form data. */
		int start_offset = boundary->offsets[i] + BOUNDARY_LENGTH + 40;

		/* End so that there is atleast BOUNDARY_LENGTH chars to
		 * compare. Subtract 2 char because there is no need to also
		 * compare the '--' prefix that is part of the boundary. */
		int end_offset = boundary->offsets[i + 1] - BOUNDARY_LENGTH - 2;
		unsigned char *pos = data->source + start_offset;
		unsigned char *end = data->source + end_offset;

		for (; pos <= end; pos++) {
			if (memcmp(pos, bound, BOUNDARY_LENGTH))
				continue;

			/* If incrementing causes overflow bail out. There is
			 * no need to reset the boundary string with '0' since
			 * that is already done when incrementing. */
			if (!increment_boundary_counter(boundary))
				return;

			/* Else start checking all boundaries using the new
			 * boundary string */
			i = 0;
			break;
		}
	}

	/* Now update all the boundaries with the unique boundary string */
	for (i = 0; i < boundary->count; i++)
		memcpy(data->source + boundary->offsets[i], bound, BOUNDARY_LENGTH);
}

/** @todo FIXME: shouldn't we encode data at send time (in http.c) ? --Zas */
static void
encode_multipart(struct session *ses, LIST_OF(struct submitted_value) *l,
		 struct string *data,
		 struct boundary_info *boundary, int cp_from, int cp_to)
{
	struct conv_table *convert_table = NULL;
	struct submitted_value *sv;

	assert(ses && l && data && boundary);
	if_assert_failed return;

	init_boundary(boundary);

	foreach (sv, *l) {
		add_boundary(data, boundary);
		add_crlf_to_string(data);

		/** @bug FIXME: name is not encoded.
		 * from RFC 1867:
		 * multipart/form-data contains a series of parts.
		 * Each part is expected to contain a content-disposition
		 * header where the value is "form-data" and a name attribute
		 * specifies the field name within the form,
		 * e.g., 'content-disposition: form-data; name="xxxxx"',
		 * where xxxxx is the field name corresponding to that field.
		 * Field names originally in non-ASCII character sets may be
		 * encoded using the method outlined in RFC 1522. */
		add_to_string(data, "Content-Disposition: form-data; name=\"");
		add_to_string(data, sv->name);
		add_char_to_string(data, '"');

		if (sv->type == FC_FILE) {
#define F_BUFLEN 1024
			int fh;
			unsigned char buffer[F_BUFLEN];
			unsigned char *extension;

			add_to_string(data, "; filename=\"");
			add_to_string(data, get_filename_position(sv->value));
			/* It sends bad data if the file name contains ", but
			   Netscape does the same */
			/* FIXME: We should follow RFCs 1522, 1867,
			 * 2047 (updated by rfc 2231), to provide correct support
			 * for non-ASCII and special characters in values. --Zas */
			add_char_to_string(data, '"');

			/* Add a Content-Type header if the type is configured */
			extension = strrchr(sv->value, '.');
			if (extension) {
				unsigned char *type = get_extension_content_type(extension);

				if (type) {
					add_crlf_to_string(data);
					add_to_string(data, "Content-Type: ");
					add_to_string(data, type);
					mem_free(type);
				}
			}

			add_crlf_to_string(data);
			add_crlf_to_string(data);

			if (*sv->value) {
				unsigned char *filename;

				if (get_cmd_opt_bool("anonymous")) {
					errno = EPERM;
					goto encode_error;
				}

				/* FIXME: DO NOT COPY FILE IN MEMORY !! --Zas */
				filename = expand_tilde(sv->value);
				if (!filename) goto encode_error;

				fh = open(filename, O_RDONLY);
				mem_free(filename);

				if (fh == -1) goto encode_error;
				set_bin(fh);
				while (1) {
					ssize_t rd = safe_read(fh, buffer, F_BUFLEN);

					if (rd) {
						if (rd == -1) {
							close(fh);
							goto encode_error;
						}

						add_bytes_to_string(data, buffer, rd);

					} else {
						break;
					}
				};
				close(fh);
			}
#undef F_BUFLEN
		} else {
			add_crlf_to_string(data);
			add_crlf_to_string(data);

			/* Convert back to original encoding (see
			 * html_form_control() for the original recoding). */
			if (sv->type == FC_TEXT || sv->type == FC_PASSWORD ||
			    sv->type == FC_TEXTAREA) {
				unsigned char *p;

				if (!convert_table)
					convert_table = get_translation_table(cp_from,
									      cp_to);

				p = convert_string(convert_table, sv->value,
						   strlen(sv->value), -1, CSM_FORM, NULL,
						   NULL, NULL);
				if (p) {
					add_to_string(data, p);
					mem_free(p);
				}
			} else {
				add_to_string(data, sv->value);
			}
		}

		add_crlf_to_string(data);
	}

	/* End-boundary */
	add_boundary(data, boundary);
	add_to_string(data, "--\r\n");

	check_boundary(data, boundary);

	mem_free_if(boundary->offsets);
	return;

encode_error:
	mem_free_if(boundary->offsets);
	done_string(data);

	/* XXX: This error message should move elsewhere. --Zas */
	info_box(ses->tab->term, MSGBOX_FREE_TEXT,
		 N_("Error while posting form"), ALIGN_CENTER,
		 msg_text(ses->tab->term, N_("Could not load file %s: %s"),
			  sv->value, strerror(errno)));
}

static void
encode_newlines(struct string *string, unsigned char *data)
{
	for (; *data; data++) {
		if (*data == '\n' || *data == '\r') {
			unsigned char buffer[3];

			/* Hex it. */
			buffer[0] = '%';
			buffer[1] = hx((((int) *data) & 0xF0) >> 4);
			buffer[2] = hx(((int) *data) & 0xF);
			add_bytes_to_string(string, buffer, 3);
		} else {
			add_char_to_string(string, *data);
		}
	}
}

static void
encode_text_plain(LIST_OF(struct submitted_value) *l, struct string *data,
		  int cp_from, int cp_to)
{
	struct submitted_value *sv;
	struct conv_table *convert_table = get_translation_table(cp_from, cp_to);

	assert(l && data);
	if_assert_failed return;

	foreach (sv, *l) {
		unsigned char *area51 = NULL;
		unsigned char *value = sv->value;

		add_to_string(data, sv->name);
		add_char_to_string(data, '=');

		switch (sv->type) {
		case FC_TEXTAREA:
			value = area51 = encode_textarea(sv);
			if (!area51) break;
			/* Fall through */
		case FC_HIDDEN:
			if (!area51) value = area51 = encode_crlf(sv);
			if (!area51) break;
			/* Fall through */
		case FC_TEXT:
		case FC_PASSWORD:
			/* Convert back to original encoding (see
			 * html_form_control() for the original recoding). */
			value = convert_string(convert_table, value,
			                       strlen(value), -1, CSM_FORM,
			                       NULL, NULL, NULL);
		default:
			/* Falling right through to free that textarea stuff */
			mem_free_if(area51);

			/* Did the conversion fail? */
			if (!value) break;

			encode_newlines(data, value);

			/* Free if we did convert something */
			if (value != sv->value) mem_free(value);
		}

		add_crlf_to_string(data);
	}
}

void
do_reset_form(struct document_view *doc_view, struct form *form)
{
	struct form_control *fc;

	assert(doc_view && doc_view->document);
	if_assert_failed return;

	foreach (fc, form->items) {
		struct form_state *fs = find_form_state(doc_view, fc);

		if (fs) init_form_state(doc_view, fc, fs);
	}
}

enum frame_event_status
reset_form(struct session *ses, struct document_view *doc_view, int a)
{
	struct link *link = get_current_link(doc_view);

	if (!link) return FRAME_EVENT_OK;

	do_reset_form(doc_view, get_link_form_control(link)->form);
	draw_forms(ses->tab->term, doc_view);

	/* Could be the refresh return value and then ditch the draw_forms()
	 * call. */
	return FRAME_EVENT_OK;
}

struct uri *
get_form_uri(struct session *ses, struct document_view *doc_view,
	     struct form_control *fc)
{
	struct boundary_info boundary;
	INIT_LIST_OF(struct submitted_value, submit);
	struct string data;
	struct string go;
	int cp_from, cp_to;
	struct uri *uri;
	struct form *form;

	assert(ses && ses->tab && ses->tab->term);
	if_assert_failed return NULL;
	assert(doc_view && doc_view->document && fc && fc->form);
	if_assert_failed return NULL;

	form = fc->form;

	if (fc->type == FC_RESET) {
		do_reset_form(doc_view, form);
		return NULL;
	}

	if (!form->action
	    || !init_string(&data))
		return NULL;

	get_successful_controls(doc_view, fc, &submit);

	cp_from = get_terminal_codepage(ses->tab->term);
	cp_to = doc_view->document->cp;
	switch (form->method) {
	case FORM_METHOD_GET:
	case FORM_METHOD_POST:
		encode_controls(&submit, &data, cp_from, cp_to);
		break;

	case FORM_METHOD_POST_MP:
		encode_multipart(ses, &submit, &data, &boundary, cp_from, cp_to);
		break;

	case FORM_METHOD_POST_TEXT_PLAIN:
		encode_text_plain(&submit, &data, cp_from, cp_to);
	}

#ifdef CONFIG_FORMHIST
	/* XXX: We check data.source here because a NULL value can indicate
	 * not only a memory allocation failure, but also an error reading
	 * a file that is to be uploaded. TODO: Distinguish between
	 * these two classes of errors (is it worth it?). -- Miciah */
	if (data.source
	    && get_opt_bool("document.browse.forms.show_formhist"))
		memorize_form(ses, &submit, form);
#endif

	done_submitted_value_list(&submit);

	if (!data.source
	    || !init_string(&go)) {
		done_string(&data);
		return NULL;
	}

	switch (form->method) {
	case FORM_METHOD_GET:
	{
		unsigned char *pos = strchr(form->action, '#');

		if (pos) {
			add_bytes_to_string(&go, form->action, pos - form->action);
		} else {
			add_to_string(&go, form->action);
		}

		if (strchr(go.source, '?'))
			add_char_to_string(&go, '&');
		else
			add_char_to_string(&go, '?');

		add_string_to_string(&go, &data);

		if (pos) add_to_string(&go, pos);
		break;
	}
	case FORM_METHOD_POST:
	case FORM_METHOD_POST_MP:
	case FORM_METHOD_POST_TEXT_PLAIN:
	{
		/* Note that we end content type here by a simple '\n',
		 * replaced later by correct '\r\n' in http_send_header(). */
		int i;

		add_to_string(&go, form->action);
		add_char_to_string(&go, POST_CHAR);
		if (form->method == FORM_METHOD_POST) {
			add_to_string(&go, "application/x-www-form-urlencoded\n");

		} else if (form->method == FORM_METHOD_POST_TEXT_PLAIN) {
			/* Dunno about this one but we don't want the full
			 * hextcat thingy. --jonas */
			add_to_string(&go, "text/plain\n");
			add_to_string(&go, data.source);
			break;

		} else {
			add_to_string(&go, "multipart/form-data; boundary=");
			add_bytes_to_string(&go, boundary.string, BOUNDARY_LENGTH);
			add_char_to_string(&go, '\n');
		}

		for (i = 0; i < data.length; i++) {
			unsigned char p[3];

			ulonghexcat(p, NULL, (int) data.source[i], 2, '0', 0);
			add_to_string(&go, p);
		}
	}
	}

	done_string(&data);

	uri = get_uri(go.source, 0);
	done_string(&go);
	if (uri) uri->form = 1;

	return uri;
}

#undef BOUNDARY_LENGTH


enum frame_event_status
submit_form(struct session *ses, struct document_view *doc_view, int do_reload)
{
	goto_current_link(ses, doc_view, do_reload);
	return FRAME_EVENT_OK;
}

void
submit_given_form(struct session *ses, struct document_view *doc_view,
		  struct form *form, int do_reload)
{
/* Added support for submitting forms in hidden
 * links in 1.285, commented code can safely be removed once we have made sure the new
 * code does the right thing. */
#if 0

	struct document *document = doc_view->document;
	int link;

	for (link = 0; link < document->nlinks; link++) {
		struct form_control *fc = get_link_form_control(&document->links[link]);

		if (fc && fc->form == form) {
			doc_view->vs->current_link = link;
			submit_form(ses, doc_view, 0);
			return;
		}
	}
#endif
	if (!list_empty(form->items)) {
		struct form_control *fc = (struct form_control *)form->items.next;
		struct uri *uri;
		enum cache_mode mode = do_reload ? CACHE_MODE_FORCE_RELOAD : CACHE_MODE_NORMAL;

		if (!fc) return;
		uri = get_form_uri(ses, doc_view, fc);
		if (!uri) return;
		goto_uri_frame(ses, uri, form->target, mode);
		done_uri(uri);
	}
}

void
auto_submit_form(struct session *ses)
{
	struct document *document = ses->doc_view->document;

	if (!list_empty(document->forms))
		submit_given_form(ses, ses->doc_view, document->forms.next, 0);
}


/* menu_func_T */
static void
set_file_form_state(struct terminal *term, void *filename_, void *fs_)
{
	unsigned char *filename = filename_;
	struct form_state *fs = fs_;

	/* The menu code doesn't free the filename data */
	mem_free_set(&fs->value, filename);
	fs->state = strlen(filename);
	redraw_terminal(term);
}

/* menu_func_T */
static void
file_form_menu(struct terminal *term, void *path_, void *fs_)
{
	unsigned char *path = path_;
	struct form_state *fs = fs_;

	/* FIXME: It doesn't work for ../../ */
#if 0
	int valuelen = strlen(fs->value);
	int pathlen = strlen(path);
	int no_elevator = 0;

	/* Don't add elevators for subdirs menus */
	/* It is not perfect at all because fs->value is not updated for each
	 * newly opened file menu. Maybe it should be dropped. */
	for (; valuelen < pathlen; valuelen++) {
		if (dir_sep(path[valuelen - 1])) {
			no_elevator = 1;
			break;
		}
	}
#endif

	auto_complete_file(term, 0 /* no_elevator */, path,
			   set_file_form_state,
			   file_form_menu, fs);
}


enum frame_event_status
field_op(struct session *ses, struct document_view *doc_view,
	 struct link *link, struct term_event *ev)
{
	struct form_control *fc;
	struct form_state *fs;
	enum edit_action action_id;
	unsigned char *text;
	int length;
	enum frame_event_status status = FRAME_EVENT_REFRESH;
#ifdef CONFIG_UTF8
	const unsigned char *ctext;
	int utf8 = ses->tab->term->utf8_cp;
#endif /* CONFIG_UTF8 */

	assert(ses && doc_view && link && ev);
	if_assert_failed return FRAME_EVENT_OK;

	fc = get_link_form_control(link);
	assertm(fc != NULL, "link has no form control");
	if_assert_failed return FRAME_EVENT_OK;

	if (fc->mode == FORM_MODE_DISABLED || ev->ev != EVENT_KBD
	    || ses->insert_mode == INSERT_MODE_OFF)
		return FRAME_EVENT_IGNORED;

	action_id = kbd_action(KEYMAP_EDIT, ev, NULL);

	fs = find_form_state(doc_view, fc);
	if (!fs || !fs->value) return FRAME_EVENT_OK;

	switch (action_id) {
		case ACT_EDIT_LEFT:
#ifdef CONFIG_UTF8
			if (fc->type == FC_TEXTAREA) {
				status = textarea_op_left(fs, fc, utf8);
				break;
			}
			if (utf8) {
				unsigned char *new_value;

				new_value = utf8_prevchar(fs->value + fs->state, 1, fs->value);
				fs->state = new_value - fs->value;
			} else
#endif /* CONFIG_UTF8 */
				fs->state = int_max(fs->state - 1, 0);
			break;
		case ACT_EDIT_RIGHT:
#ifdef CONFIG_UTF8
			if (fc->type == FC_TEXTAREA) {
				status = textarea_op_right(fs, fc, utf8);
				break;
			}
			if (utf8) {
				unsigned char *text = fs->value + fs->state;
				unsigned char *end = strchr(text, '\0');

				utf8_to_unicode(&text, end);
				fs->state = (int)(text - fs->value);
			} else
#endif /* CONFIG_UTF8 */
				fs->state = int_min(fs->state + 1, strlen(fs->value));
			break;
		case ACT_EDIT_HOME:
#ifdef CONFIG_UTF8
			if (fc->type == FC_TEXTAREA) {
				status = textarea_op_home(fs, fc, utf8);
			} else {
				fs->state = 0;
			}
#else
			if (fc->type == FC_TEXTAREA) {
				status = textarea_op_home(fs, fc);
			} else {
				fs->state = 0;
			}

#endif /* CONFIG_UTF8 */
			break;
		case ACT_EDIT_UP:
			if (fc->type != FC_TEXTAREA)
				status = FRAME_EVENT_IGNORED;
			else
#ifdef CONFIG_UTF8
				status = textarea_op_up(fs, fc, utf8);
#else
				status = textarea_op_up(fs, fc);
#endif /* CONFIG_UTF8 */
			break;
		case ACT_EDIT_DOWN:
			if (fc->type != FC_TEXTAREA)
				status = FRAME_EVENT_IGNORED;
			else
#ifdef CONFIG_UTF8
				status = textarea_op_down(fs, fc, utf8);
#else
				status = textarea_op_down(fs, fc);
#endif /* CONFIG_UTF8 */
			break;
		case ACT_EDIT_END:
			if (fc->type == FC_TEXTAREA) {
#ifdef CONFIG_UTF8
				status = textarea_op_end(fs, fc, utf8);
#else
				status = textarea_op_end(fs, fc);
#endif /* CONFIG_UTF8 */
			} else {
				fs->state = strlen(fs->value);
			}
			break;
		case ACT_EDIT_BEGINNING_OF_BUFFER:
			if (fc->type == FC_TEXTAREA) {
#ifdef CONFIG_UTF8
				status = textarea_op_bob(fs, fc, utf8);
				fs->state_cell = 0;
#else
				status = textarea_op_bob(fs, fc);
#endif /* CONFIG_UTF8 */
			} else {
				fs->state = 0;
			}
			break;
		case ACT_EDIT_END_OF_BUFFER:
			if (fc->type == FC_TEXTAREA) {
#ifdef CONFIG_UTF8
				status = textarea_op_eob(fs, fc, utf8);
#else
				status = textarea_op_eob(fs, fc);
#endif /* CONFIG_UTF8 */
			} else {
				fs->state = strlen(fs->value);
			}
			break;
		case ACT_EDIT_OPEN_EXTERNAL:
			if (form_field_is_readonly(fc))
				status = FRAME_EVENT_IGNORED;
			else if (fc->type == FC_TEXTAREA)
				textarea_edit(0, ses->tab->term, fs, doc_view, link);
			break;
		case ACT_EDIT_COPY_CLIPBOARD:
			set_clipboard_text(fs->value);
			status = FRAME_EVENT_OK;
			break;
		case ACT_EDIT_CUT_CLIPBOARD:
			set_clipboard_text(fs->value);
			if (!form_field_is_readonly(fc))
				fs->value[0] = 0;
			fs->state = 0;
#ifdef CONFIG_UTF8
			if (fc->type == FC_TEXTAREA)
				fs->state_cell = 0;
#endif /* CONFIG_UTF8 */
			break;
		case ACT_EDIT_PASTE_CLIPBOARD:
			if (form_field_is_readonly(fc)) break;

			text = get_clipboard_text();
			if (!text) break;

			length = strlen(text);
			if (length <= fc->maxlength) {
				unsigned char *v = mem_realloc(fs->value, length + 1);

				if (v) {
					fs->value = v;
					memmove(v, text, length + 1);
					fs->state = strlen(fs->value);
#ifdef CONFIG_UTF8
					if (utf8 && fc->type == FC_TEXTAREA)
						fs->state_cell = 0;
#endif /* CONFIG_UTF8 */
				}
			}
			mem_free(text);
			break;
		case ACT_EDIT_ENTER:
			if (fc->type == FC_TEXTAREA) {
#ifdef CONFIG_UTF8
				status = textarea_op_enter(fs, fc, utf8);
#else
				status = textarea_op_enter(fs, fc);
#endif /* CONFIG_UTF8 */
				break;
			}

			/* Set status to ok if either it is not possible to
			 * submit the form or the posting fails. */
			/* FIXME: We should maybe have ACT_EDIT_ENTER_RELOAD */
			if ((has_form_submit(fc->form)
			      && !get_opt_bool("document.browse.forms.auto_submit"))
			    || goto_current_link(ses, doc_view, 0)) {
				if (ses->insert_mode == INSERT_MODE_ON)
					ses->insert_mode = INSERT_MODE_OFF;
				status = FRAME_EVENT_OK;
			}
			break;
		case ACT_EDIT_BACKSPACE:
			if (form_field_is_readonly(fc)) {
				status = FRAME_EVENT_IGNORED;
				break;
			}

			if (!fs->state) {
				status = FRAME_EVENT_OK;
				break;
			}
#ifdef CONFIG_UTF8
			if (utf8) {
				int old_state = fs->state;
				unsigned char *new_value;

				new_value = utf8_prevchar(fs->value + fs->state, 1, fs->value);
				fs->state = new_value - fs->value;

				if (old_state != fs->state) {
					if (fc->type == FC_TEXTAREA)
						fs->state_cell = 0;
					length = strlen(fs->value + old_state) + 1;
					memmove(new_value, fs->value + old_state, length);
				}
			} else
#endif /* CONFIG_UTF8 */
			{
				length = strlen(fs->value + fs->state) + 1;
				text = fs->value + fs->state;

				memmove(text - 1, text, length);
				fs->state--;
			}
			break;
		case ACT_EDIT_DELETE:
			if (form_field_is_readonly(fc)) {
				status = FRAME_EVENT_IGNORED;
				break;
			}

			length = strlen(fs->value);
			if (fs->state >= length) {
				status = FRAME_EVENT_OK;
				break;
			}
#ifdef CONFIG_UTF8
			if (utf8) {
				unsigned char *end = fs->value + length;
				unsigned char *text = fs->value + fs->state;
				unsigned char *old = text;

				utf8_to_unicode(&text, end);
				if (old != text) {
					memmove(old, text,
						(int)(end - text) + 1);
				}
				break;
			}
#endif /* CONFIG_UTF8 */
			text = fs->value + fs->state;

			memmove(text, text + 1, length - fs->state);
			break;
		case ACT_EDIT_KILL_TO_BOL:
			if (form_field_is_readonly(fc)) {
				status = FRAME_EVENT_IGNORED;
				break;
			}

			if (fs->state <= 0) {
				status = FRAME_EVENT_OK;
				break;
			}

			text = memrchr(fs->value, ASCII_LF, fs->state);
			if (text) {
				/* Leave the new-line character if it does not
				 * immediately precede the cursor. */
				if (text != &fs->value[fs->state - 1])
					text++;
			} else {
				text = fs->value;
			}

			length = strlen(fs->value + fs->state) + 1;
			memmove(text, fs->value + fs->state, length);

			fs->state = (int) (text - fs->value);
#ifdef CONFIG_UTF8
			if (utf8) {
				if (fc->type == FC_TEXTAREA)
					fs->state_cell = 0;
			}
#endif /* CONFIG_UTF8 */
			break;
		case ACT_EDIT_KILL_TO_EOL:
			if (form_field_is_readonly(fc)) {
				status = FRAME_EVENT_IGNORED;
				break;
			}

			if (!fs->value[fs->state]) {
				status = FRAME_EVENT_OK;
				break;
			}

			text = strchr(fs->value + fs->state, ASCII_LF);
			if (!text) {
				fs->value[fs->state] = '\0';
				break;
			}

			if (fs->value[fs->state] == ASCII_LF)
				++text;

			memmove(fs->value + fs->state, text, strlen(text) + 1);
			break;

		case ACT_EDIT_KILL_WORD_BACK:
			if (form_field_is_readonly(fc)) {
				status = FRAME_EVENT_IGNORED;
				break;
			}

			if (fs->state <= 0) {
				status = FRAME_EVENT_OK;
				break;
			}

			text = &fs->value[fs->state];
			while (text > fs->value && isspace(*(text - 1)))
				--text;
			while (text > fs->value && !isspace(*(text - 1)))
				--text;
			if (*text == ASCII_LF
			    && text != &fs->value[fs->state - 1])
				text++;

			length = strlen(fs->value + fs->state) + 1;
			memmove(text, fs->value + fs->state, length);

			fs->state = (int) (text - fs->value);
			break;

		case ACT_EDIT_MOVE_BACKWARD_WORD:
			while (fs->state > 0
			       && isspace(fs->value[fs->state - 1]))
				--fs->state;
			while (fs->state > 0
			       && !isspace(fs->value[fs->state - 1]))
				--fs->state;
			break;

		case ACT_EDIT_MOVE_FORWARD_WORD:
			while (isspace(fs->value[fs->state]))
				++fs->state;
			while (fs->value[fs->state]
			       && !isspace(fs->value[fs->state]))
				++fs->state;
			while (isspace(fs->value[fs->state]))
				++fs->state;
			break;

		case ACT_EDIT_AUTO_COMPLETE:
			if (fc->type != FC_FILE
			    || form_field_is_readonly(fc)) {
				status = FRAME_EVENT_IGNORED;
				break;
			}

			file_form_menu(ses->tab->term, fs->value, fs);
			break;

		case ACT_EDIT_CANCEL:
			if (ses->insert_mode == INSERT_MODE_ON)
				ses->insert_mode = INSERT_MODE_OFF;
			else
				status = FRAME_EVENT_IGNORED;
			break;

		case ACT_EDIT_REDRAW:
			redraw_terminal_cls(ses->tab->term);
			status = FRAME_EVENT_OK;
			break;

		default:
			if (!check_kbd_textinput_key(ev)) {
				status = FRAME_EVENT_IGNORED;
				break;
			}

			if (form_field_is_readonly(fc)
#ifndef CONFIG_UTF8
					|| strlen(fs->value) >= fc->maxlength
					|| !insert_in_string(&fs->value, fs->state, "?", 1)
#endif /* CONFIG_UTF8 */
					)
					{
				status = FRAME_EVENT_OK;
				break;
			}

#ifdef CONFIG_UTF8
			/* fs->value is in the charset of the terminal.  */
			ctext = u2cp_no_nbsp(get_kbd_key(ev),
					     get_terminal_codepage(ses->tab->term));
			length = strlen(ctext);

			if (strlen(fs->value) + length > fc->maxlength
			    || !insert_in_string(&fs->value, fs->state, ctext, length)) {
				status = FRAME_EVENT_OK;
				break;
			}

			fs->state += length;
			if (fc->type == FC_TEXTAREA)
				fs->state_cell = 0;
#else
			fs->value[fs->state++] = get_kbd_key(ev);
#endif /* CONFIG_UTF8 */
			break;
	}

	return status;
}

static unsigned char *
get_form_label(struct form_control *fc)
{
	assert(fc->form);
	switch (fc->type) {
	case FC_RESET:
		return N_("Reset form");
	case FC_BUTTON:
		return N_("Harmless button");
	case FC_HIDDEN:
		return NULL;
	case FC_SUBMIT:
	case FC_IMAGE:
		if (!fc->form->action) return NULL;

		if (fc->form->method == FORM_METHOD_GET)
			return N_("Submit form to");
		return N_("Post form to");
	case FC_RADIO:
		return N_("Radio button");
	case FC_CHECKBOX:
		return N_("Checkbox");
	case FC_SELECT:
		return N_("Select field");
	case FC_TEXT:
		return N_("Text field");
	case FC_TEXTAREA:
		return N_("Text area");
	case FC_FILE:
		return N_("File upload");
	case FC_PASSWORD:
		return N_("Password field");
	}

	return NULL;
}

static inline void
add_form_attr_to_string(struct string *string, struct terminal *term,
			unsigned char *name, unsigned char *value)
{
	add_to_string(string, ", ");
	add_to_string(string, _(name, term));
	if (value) {
		add_char_to_string(string, ' ');
		add_to_string(string, value);
	}
}

unsigned char *
get_form_info(struct session *ses, struct document_view *doc_view)
{
	struct terminal *term = ses->tab->term;
	struct link *link = get_current_link(doc_view);
	struct form_control *fc;
	unsigned char *label, *key;
	struct string str;

	assert(link);

	fc = get_link_form_control(link);
	label = get_form_label(fc);
	if (!label) return NULL;

	if (!init_string(&str)) return NULL;

	add_to_string(&str, _(label, term));

	if (link->type != LINK_BUTTON && fc->name && fc->name[0]) {
		add_form_attr_to_string(&str, term, N_("name"), fc->name);
	}

	switch (fc->type) {
	case FC_CHECKBOX:
	case FC_RADIO:
	{
		struct form_state *fs = find_form_state(doc_view, fc);

		if (!fs->value || !fs->value[0])
			break;

		add_form_attr_to_string(&str, term, N_("value"), fs->value);
		break;
	}

	case FC_TEXT:
	case FC_PASSWORD:
	case FC_FILE:
	case FC_TEXTAREA:
	{
		struct uri *uri;
		unsigned char *uristring;

		if (form_field_is_readonly(fc)) {
			add_form_attr_to_string(&str, term, N_("read only"), NULL);
		}

		/* Should we add info about entering insert mode or add info
		 * about submitting the form? */
		if (ses->insert_mode == INSERT_MODE_OFF) {
			key = get_keystroke(ACT_EDIT_ENTER, KEYMAP_EDIT);

			if (!key) break;

			if (form_field_is_readonly(fc))
				label = N_("press %s to navigate");
			else
				label = N_("press %s to edit");

			add_to_string(&str, " (");
			add_format_to_string(&str, _(label, term), key);
			add_char_to_string(&str, ')');
			mem_free(key);
			break;

		}

		if (fc->type == FC_TEXTAREA)
			break;

		assert(fc->form);

		if (!fc->form->action
		    || (has_form_submit(fc->form)
		        && !get_opt_bool("document.browse.forms.auto_submit")))
			break;

		uri = get_uri(fc->form->action, 0);
		if (!uri) break;

		/* Add the uri with password and post info stripped */
		uristring = get_uri_string(uri, URI_PUBLIC);
		done_uri(uri);

		if (!uristring) break;

		key = get_keystroke(ACT_EDIT_ENTER, KEYMAP_EDIT);
		if (!key) {
			mem_free(uristring);
			break;
		}

		if (fc->form->method == FORM_METHOD_GET)
			label = N_("press %s to submit to %s");
		else
			label = N_("press %s to post to %s");

		add_to_string(&str, " (");
		add_format_to_string(&str, _(label, term), key, uristring);
		mem_free(uristring);
		mem_free(key);

		add_char_to_string(&str, ')');
		break;
	}
	case FC_SUBMIT:
	case FC_IMAGE:
		add_char_to_string(&str, ' ');

		assert(fc->form);
		/* Add the uri with password and post info stripped */
		add_string_uri_to_string(&str, fc->form->action, URI_PUBLIC);
		break;

	case FC_HIDDEN:
	case FC_RESET:
	case FC_BUTTON:
	case FC_SELECT:
		break;
	}

	if (link->accesskey
	    && get_opt_bool("document.browse.accesskey.display")) {
		add_to_string(&str, " (");
		add_accesskey_to_string(&str, link->accesskey);
		add_char_to_string(&str, ')');
	}

	return str.source;
}

static void
link_form_menu_func(struct terminal *term, void *link_number_, void *ses_)
{
	struct session *ses = ses_;
	struct document_view *doc_view;
	int link_number = *(int *) link_number_;

	mem_free(link_number_);

	assert(term && ses);
	if_assert_failed return;

	doc_view = current_frame(ses);
	if (!doc_view) return;

	assert(doc_view->vs && doc_view->document);
	if_assert_failed return;

	jump_to_link_number(ses, doc_view, link_number);
	refresh_view(ses, doc_view, 0);
}

void
link_form_menu(struct session *ses)
{
	struct document_view *doc_view;
	struct link *link;
	struct menu_item *mi;
	struct form_control *fc;
	struct form *form;

	assert(ses);
	if_assert_failed return;

	doc_view = current_frame(ses);
	if (!doc_view) return;

	assert(doc_view->vs && doc_view->document);
	if_assert_failed return;

	link = get_current_link(doc_view);
	if (!link) return;

	assert(link_is_form(link));

	fc = get_link_form_control(link);
	if (!fc) return;

	form = fc->form;

	mi = new_menu(FREE_LIST | FREE_TEXT | NO_INTL);
	if (!mi) return;

	foreach (fc, form->items) {
		unsigned char *text;
		unsigned char *rtext;
		int link_number;
		struct string str;

		switch (fc->type) {
		case FC_HIDDEN:
			continue;

		case FC_SUBMIT:
		case FC_IMAGE:
			if (!form->action)
				text = N_("Useless button");
			else
				text = N_("Submit button");
			break;

		default:
			text = get_form_label(fc);
		}

		link_number = get_form_control_link(doc_view->document, fc);
		if (link_number < 0
		    || !init_string(&str))
			continue;

		assert(text);
		add_to_string(&str, _(text, ses->tab->term));

		rtext = fc->name;
		if (!rtext) rtext = fc->alt;

		add_to_menu(&mi, str.source, rtext, ACT_MAIN_NONE,
		            link_form_menu_func, intdup(link_number),
		            FREE_DATA);
	}

	do_menu(ses->tab->term, mi, ses, 1);
}
