/** Textarea form item handlers
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we want memrchr() ! */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "document/document.h"
#include "document/forms.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/action.h"
#include "viewer/text/form.h"
#include "viewer/text/textarea.h"
#include "viewer/text/view.h"


struct line_info {
	int start;
	int end;
#ifdef CONFIG_UTF8
	int last_char_width;
	int split_prev:1;
	int split_next:1;
#endif /* CONFIG_UTF8 */
};

/** We add two extra entries to the table so the ending info can be added
 * without reallocating. */
#define realloc_line_info(info, size) \
	mem_align_alloc(info, size, (size) + 3, 0xFF)

#ifdef CONFIG_UTF8
/** Allocates a line_info table describing the layout of the textarea buffer.
 *
 * @param text		the text to format; must be in UTF-8
 * @param width		is max width and the offset at which @a text will be
 *			wrapped
 * @param wrap		controls how the wrapping of @a text is performed
 * @param format	is non zero the @a text will be modified to make it
 *			suitable for encoding it for form posting
 */
static struct line_info *
format_textutf8(unsigned char *text, int width, enum form_wrap wrap, int format)
{
	struct line_info *line = NULL;
	int line_number = 0;
	int begin = 0;
	int pos = 0;
	unsigned char *text_end;
	int skip;
	unsigned char *wrappos=NULL;
	int chars_cells=0; /* Number of console chars on line */

	assert(text);
	if_assert_failed return NULL;

	/* Allocate the ending entries */
	if (!realloc_line_info(&line, 0))
		return NULL;

	text_end = text + strlen(text);
	while (text[pos]) {
		int char_cells = utf8_char2cells(&text[pos], text_end);

		if (text[pos] == ' ')
			wrappos = &text[pos];

		if (text[pos] == '\n') {
			skip = 1;

		} else if (wrap == FORM_WRAP_NONE || chars_cells + char_cells < width) {
			pos += utf8charlen(&text[pos]);
			chars_cells += char_cells;
			continue;

		} else {
			if (wrappos) {
				/* When formatting text for form submitting we
				 * have to apply the wrapping mode. */
				if (wrap == FORM_WRAP_HARD && format)
					*wrappos = '\n';
				pos = wrappos - text;
			}
			skip = !!wrappos;
		}

		if (!realloc_line_info(&line, line_number)) {
			mem_free_if(line);
			return NULL;
		}

		line[line_number].last_char_width = char_cells;
		line[line_number].split_next = !skip;
		line[line_number].start = begin;
		line[line_number++].end = pos;
		line[line_number].split_prev = !skip;

		begin = pos += skip;

		chars_cells = 0;
		wrappos = NULL;
	}

	line[line_number].split_next = 0;

	/* Flush the last text before the loop ended */
	line[line_number].start = begin;
	line[line_number++].end = pos;

	/* Add end marker */
	line[line_number].start = line[line_number].end = -1;

	line[line_number].split_next = line[line_number].split_prev = 0;
	line[0].split_prev = 0;

	return line;
}
#endif /* CONFIG_UTF8 */

/** Allocates a line_info table describing the layout of the textarea buffer.
 *
 * @param text		the text to format; must be in a unibyte charset
 * @param width		is max width and the offset at which @a text will be
 *			wrapped
 * @param wrap		controls how the wrapping of @a text is performed
 * @param format	is non zero the @a text will be modified to make it
 *			suitable for encoding it for form posting
 */
static struct line_info *
format_text(unsigned char *text, int width, enum form_wrap wrap, int format)
{
	struct line_info *line = NULL;
	int line_number = 0;
	int begin = 0;
	int pos = 0;
	int skip;

	assert(text);
	if_assert_failed return NULL;

	/* Allocate the ending entries */
	if (!realloc_line_info(&line, 0))
		return NULL;

	while (text[pos]) {
		if (text[pos] == '\n') {
			skip = 1;

		} else if (wrap == FORM_WRAP_NONE || pos - begin < width) {
			pos++;
			continue;

		} else {
			unsigned char *wrappos;

			/* Find a place to wrap the text */
			wrappos = memrchr(&text[begin], ' ', pos - begin);
			if (wrappos) {
				/* When formatting text for form submitting we
				 * have to apply the wrapping mode. */
				if (wrap == FORM_WRAP_HARD && format)
					*wrappos = '\n';
				pos = wrappos - text;
			}
			skip = !!wrappos;
		}

		if (!realloc_line_info(&line, line_number)) {
			mem_free_if(line);
			return NULL;
		}

		line[line_number].start = begin;
		line[line_number++].end = pos;
		begin = pos += skip;
	}

	/* Flush the last text before the loop ended */
	line[line_number].start = begin;
	line[line_number++].end = pos;

	/* Add end marker */
	line[line_number].start = line[line_number].end = -1;

	return line;
}

/** Searches for @a cursor_position (aka. position in the
 * form_state.value string) for the corresponding entry in the @a line
 * info. Returns the index or -1 if position is not found. */
static int
get_textarea_line_number(struct line_info *line, int cursor_position)
{
	int idx;

	for (idx = 0; line[idx].start != -1; idx++) {
		int wrap;

		if (cursor_position < line[idx].start) continue;

		wrap = (line[idx + 1].start == line[idx].end);
		if (cursor_position >= line[idx].end + !wrap) continue;

		return idx;
	}

	return -1;
}

/** Fixes up the form_state.vpos and form_state.vypos members.
 * @returns the logical position in the textarea view. */
#ifdef CONFIG_UTF8
int
area_cursor(struct form_control *fc, struct form_state *fs, int utf8)
{
	struct line_info *line;
	int x, y;

	assert(fc && fs);
	if_assert_failed return 0;

	if (utf8)
		line = format_textutf8(fs->value, fc->cols, fc->wrap, 0);
	else
		line = format_text(fs->value, fc->cols, fc->wrap, 0);

	if (!line) return 0;

	if (fs->state_cell)
		y = get_textarea_line_number(line, fs->state_cell);
	else
		y = get_textarea_line_number(line, fs->state);

	if (y == -1) {
		mem_free(line);
		return 0;
	}

	if (utf8) {
		if (fs->state_cell) {
			x = utf8_ptr2cells(fs->value + line[y].start,
					   fs->value + fs->state_cell);
			x += line[y].last_char_width;
		} else
			x = utf8_ptr2cells(fs->value + line[y].start,
					   fs->value + fs->state);
	} else {
		x = fs->state - line[y].start;
		if (fc->wrap && x == fc->cols) x--;
	}

	mem_free(line);

	int_bounds(&fs->vpos, x - fc->cols + 1, x);
	int_bounds(&fs->vypos, y - fc->rows + 1, y);

	x -= fs->vpos;
	y -= fs->vypos;

	return y * fc->cols + x;
}

#else

int
area_cursor(struct form_control *fc, struct form_state *fs)
{
	struct line_info *line;
	int x, y;

	assert(fc && fs);
	if_assert_failed return 0;

	line = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!line) return 0;

	y = get_textarea_line_number(line, fs->state);
	if (y == -1) {
		mem_free(line);
		return 0;
	}

	x = fs->state - line[y].start;

	mem_free(line);

	if (fc->wrap && x == fc->cols) x--;

	int_bounds(&fs->vpos, x - fc->cols + 1, x);
	int_bounds(&fs->vypos, y - fc->rows + 1, y);

	x -= fs->vpos;
	y -= fs->vypos;

	return y * fc->cols + x;
}
#endif

#ifdef CONFIG_UTF8
static void
draw_textarea_utf8(struct terminal *term, struct form_state *fs,
	      struct document_view *doc_view, struct link *link)
{
	struct line_info *line, *linex;
	struct form_control *fc;
	struct box *box;
	int vx, vy;
	int sl, ye;
	int x, xbase, y;

	assert(term && doc_view && doc_view->document && doc_view->vs && link);
	if_assert_failed return;
	fc = get_link_form_control(link);
	assertm(fc != NULL, "link %d has no form control", (int) (link - doc_view->document->links));
	if_assert_failed return;

	box = &doc_view->box;
	vx = doc_view->vs->x;
	vy = doc_view->vs->y;

	if (!link->npoints) return;
	area_cursor(fc, fs, 1);
	linex = format_textutf8(fs->value, fc->cols, fc->wrap, 0);
	if (!linex) return;
	line = linex;
	sl = fs->vypos;
	while (line->start != -1 && sl) sl--, line++;

	xbase = link->points[0].x + box->x - vx;
	y = link->points[0].y + box->y - vy;
	ye = y + fc->rows;

	for (; line->start != -1 && y < ye; line++, y++) {
		int i;
		unsigned char *text, *end;

		text = fs->value + line->start;
		end = fs->value + line->end;

		text += utf8_cells2bytes(text, fs->vpos, end);

		if (!row_is_in_box(box, y)) continue;

		for (i = 0, x = xbase; i < fc->cols; i++, x++) {
			unicode_val_T data;

			if (i >= -fs->vpos && text < end) {
				/* utf8_to_unicode will increment text. */
				data = utf8_to_unicode(&text, end);
			} else
				data = '_';

			if (col_is_in_box(box, x)) {
				int cell = unicode_to_cell(data);

				if (cell == 2) {
					draw_char_data(term, x++, y, data);
					i++;
					data = UCS_NO_CHAR;
				}

				draw_char_data(term, x, y, data);
			}
		}
	}

	for (; y < ye; y++) {
		int i;

		if (!row_is_in_box(box, y)) continue;

		for (i = 0, x = xbase; i < fc->cols; i++, x++) {
			if (col_is_in_box(box, x))
				draw_char_data(term, x, y, '_');
		}
	}

	mem_free(linex);
}
#endif /* CONFIG_UTF8 */

void
draw_textarea(struct terminal *term, struct form_state *fs,
	      struct document_view *doc_view, struct link *link)
{
	struct line_info *line, *linex;
	struct form_control *fc;
	struct box *box;
	int vx, vy;
	int sl, ye;
	int x, y;

	assert(term && doc_view && doc_view->document && doc_view->vs && link);
	if_assert_failed return;

#ifdef CONFIG_UTF8
	if (term->utf8_cp) {
		draw_textarea_utf8(term, fs, doc_view, link);
		return;
	}
#endif /* CONFIG_UTF8 */
	fc = get_link_form_control(link);
	assertm(fc != NULL, "link %d has no form control", (int) (link - doc_view->document->links));
	if_assert_failed return;

	box = &doc_view->box;
	vx = doc_view->vs->x;
	vy = doc_view->vs->y;

	if (!link->npoints) return;
#ifdef CONFIG_UTF8
	area_cursor(fc, fs, 0);
#else
	area_cursor(fc, fs);
#endif /* CONFIG_UTF8 */
	linex = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!linex) return;
	line = linex;
	sl = fs->vypos;
	while (line->start != -1 && sl) sl--, line++;

	x = link->points[0].x + box->x - vx;
	y = link->points[0].y + box->y - vy;
	ye = y + fc->rows;

	for (; line->start != -1 && y < ye; line++, y++) {
		int i;

		if (!row_is_in_box(box, y)) continue;

		for (i = 0; i < fc->cols; i++) {
			unsigned char data;
			int xi = x + i;

			if (!col_is_in_box(box, xi))
				continue;

			if (i >= -fs->vpos
			    && i + fs->vpos < line->end - line->start)
				data = fs->value[line->start + i + fs->vpos];
			else
				data = '_';

			draw_char_data(term, xi, y, data);
		}
	}

	for (; y < ye; y++) {
		int i;

		if (!row_is_in_box(box, y)) continue;

		for (i = 0; i < fc->cols; i++) {
			int xi = x + i;

			if (col_is_in_box(box, xi))
				draw_char_data(term, xi, y, '_');
		}
	}

	mem_free(linex);
}


unsigned char *
encode_textarea(struct submitted_value *sv)
{
	struct form_control *fc;
	void *blabla;

	assert(sv && sv->value);
	if_assert_failed return NULL;

	fc = sv->form_control;

	/* We need to reformat text now if it has to be wrapped hard, just
	 * before encoding it. */
	/* TODO: Do we need here UTF-8 format or not? --scrool */
	blabla = format_text(sv->value, fc->cols, fc->wrap, 1);
	mem_free_if(blabla);

	return encode_crlf(sv);
}


/** We use some evil hacking in order to make external textarea editor working.
 * We need to have some way how to be notified that the editor finished and we
 * should reload content of the textarea.  So we use global variable
 * @c textarea_editor as a flag whether we have one running, and if we have, we
 * just call textarea_edit(1, ...).  Then we recover our state from static
 * variables, reload content of textarea back from file and clean up.
 *
 * Unfortunately, we can't support calling of editor from non-master links
 * session, as it would be extremely ugly to hack (you would have to transfer
 * the content of it back to master somehow, add special flags for not deleting
 * of 'delete' etc) and I'm not going to do that now. Inter-links communication
 * *NEEDS* rewrite, as it looks just like quick messy hack now. --pasky */
int textarea_editor = 0;

static unsigned char *
save_textarea_file(unsigned char *value)
{
	unsigned char *filename;
	FILE *fp = NULL;
	int fd;
	size_t nmemb, len;

	filename = get_tempdir_filename("elinks-area-XXXXXX");
	if (!filename) return NULL;

	fd = safe_mkstemp(filename);
	if (fd < 0) {
		mem_free(filename);
		return NULL;
	}

	len = strlen(value);
	if (len == 0) return filename;

	fp = fdopen(fd, "w");
	if (!fp) {

error:
		unlink(filename);
		mem_free(filename);
		close(fd);
		return NULL;
	}

	nmemb = fwrite(value, len, 1, fp);
	if (nmemb != 1) {
		fclose(fp);
		goto error;
	}

	if (fclose(fp) != 0)
		goto error;

	return filename;
}

void
textarea_edit(int op, struct terminal *term_, struct form_state *fs_,
	      struct document_view *doc_view_, struct link *link_)
{
	static size_t fc_maxlength;
	static struct form_state *fs;
	static struct terminal *term;
	static struct document_view *doc_view;
	static struct link *link;
	static unsigned char *fn;

	assert (op == 0 || op == 1);
	if_assert_failed return;
	assert (op == 1 || term_);
	if_assert_failed return;

	if (op == 0 && get_cmd_opt_bool("anonymous")) {
		info_box(term_, 0, N_("Error"), ALIGN_CENTER,
			 N_("You cannot launch an external"
			    " editor in the anonymous mode."));
		return;
	}

	if (op == 0 && !term_->master) {
		info_box(term_, 0, N_("Error"), ALIGN_CENTER,
			 N_("You can do this only on the master terminal"));
		return;
	}

	if (op == 0 && !textarea_editor) {
		unsigned char *ed = get_opt_str("document.browse.forms.editor");
		unsigned char *ex;

		fn = save_textarea_file(fs_->value);
		if (!fn) return;

		if (!ed || !*ed) {
			ed = getenv("EDITOR");
			if (!ed || !*ed) ed = "vi";
		}

		ex = straconcat(ed, " ", fn, (unsigned char *) NULL);
		if (!ex) {
			unlink(fn);
			goto free_and_return;
		}

		if (fs_) fs = fs_;
		if (doc_view_) doc_view = doc_view_;
		if (link_) {
			link = link_;
			fc_maxlength = get_link_form_control(link_)->maxlength;
		}
		if (term_) term = term_;

		exec_on_terminal(term, ex, "", TERM_EXEC_FG);
		mem_free(ex);

		textarea_editor = 1;

	} else if (op == 1 && fs) {
		struct string file;

		if (!init_string(&file)
		     || !add_file_to_string(&file, fn)) {
			textarea_editor = 0;
			goto free_and_return;
		}

		if (file.length > fc_maxlength) {
			file.source[fc_maxlength] = '\0';
			/* Casting size_t fc_maxlength to unsigned int
			 * and formatting it with "%u" is safe,
			 * because fc_maxlength is smaller than
			 * file.length, which is an int.  */
			info_box(term, MSGBOX_FREE_TEXT, N_("Warning"),
			         ALIGN_CENTER,
			         msg_text(term,
				          N_("You have exceeded the textarea's"
				             " size limit: your input is %d"
					     " bytes, but the maximum is %u"
					     " bytes.\n\n"
					     "Your input has been truncated,"
					     " but you can still recover the"
					     " text that you entered from"
					     " this file: %s"), file.length,
				             (unsigned int) fc_maxlength, fn));
		} else {
			unlink(fn);
		}

		mem_free(fs->value);
		fs->value = file.source;
		fs->state = file.length;

		if (doc_view && link)
			draw_form_entry(term, doc_view, link);

		textarea_editor = 0;
		goto free_and_return;
	}

	return;

free_and_return:
	mem_free_set(&fn, NULL);
	fs = NULL;
}

/* menu_func_T */
void
menu_textarea_edit(struct terminal *term, void *xxx, void *ses_)
{
	struct session *ses = ses_;
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
	if (!link) return;

	fc = get_link_form_control(link);
	if (form_field_is_readonly(fc))
		return;

	fs = find_form_state(doc_view, fc);
	if (!fs) return;

	textarea_edit(0, term, fs, doc_view, link);
}

#ifdef CONFIG_UTF8
static enum frame_event_status
textarea_op(struct form_state *fs, struct form_control *fc, int utf8,
	    int (*do_op)(struct form_state *, struct line_info *, int, int))
{
	struct line_info *line;
	int current, state;
	int state_cell;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	if (utf8)
		line = format_textutf8(fs->value, fc->cols, fc->wrap, 0);
	else
		line = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!line) return FRAME_EVENT_OK;

	current = get_textarea_line_number(line, fs->state);
	state = fs->state;
	state_cell = fs->state_cell;
	if (do_op(fs, line, current, utf8)) {
		mem_free(line);
		return FRAME_EVENT_IGNORED;
	}

	mem_free(line);
	return (fs->state == state && fs->state_cell == state_cell)
		? FRAME_EVENT_OK : FRAME_EVENT_REFRESH;
}

#else

static enum frame_event_status
textarea_op(struct form_state *fs, struct form_control *fc,
	    int (*do_op)(struct form_state *, struct line_info *, int))
{
	struct line_info *line;
	int current, state;

	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	line = format_text(fs->value, fc->cols, fc->wrap, 0);
	if (!line) return FRAME_EVENT_OK;

	current = get_textarea_line_number(line, fs->state);
	state = fs->state;
	if (do_op(fs, line, current)) {
		mem_free(line);
		return FRAME_EVENT_IGNORED;
	}

	mem_free(line);

	return fs->state == state ? FRAME_EVENT_OK : FRAME_EVENT_REFRESH;
}
#endif /* CONFIG_UTF8 */

#ifdef CONFIG_UTF8
void
new_pos(struct form_state *fs, struct line_info *line, int current, int max_cells)
{
	unsigned char *text = fs->value + line[current].start;
	unsigned char *end = fs->value + line[current].end;
	int cells = 0;

	while(cells < max_cells) {
		unicode_val_T data = utf8_to_unicode(&text, end);

		if (data == UCS_NO_CHAR) break;
		cells += unicode_to_cell(data);
	}
	fs->state = (int)(text - fs->value);
}
#endif /* CONFIG_UTF8 */

static int
#ifdef CONFIG_UTF8
do_op_home(struct form_state *fs, struct line_info *line, int current, int utf8)
#else
do_op_home(struct form_state *fs, struct line_info *line, int current)
#endif /* CONFIG_UTF8 */
{
	if (current == -1)
		return 0;
#ifdef CONFIG_UTF8
	if (utf8)
		fs->state = line[current - !!fs->state_cell].start;
	else
#endif /* CONFIG_UTF8 */
		fs->state = line[current].start;
	return 0;
}

#ifdef CONFIG_UTF8
static int
do_op_up(struct form_state *fs, struct line_info *line, int current, int utf8)
{
	int old_state;

	if (current == -1) return 0;

	if (!(current - !!fs->state_cell)) return 1;

	if (!utf8) {
		fs->state -= line[current].start - line[current-1].start;
		int_upper_bound(&fs->state, line[current-1].end);
		return 0;
	}

	old_state = fs->state;
	if (fs->state_cell) {
		int len = utf8_ptr2cells(fs->value + line[current - 1].start,
			                 fs->value + fs->state_cell);

		new_pos(fs, line, current - 2, len + line[current - 1].last_char_width);
	} else {
		int len = utf8_ptr2cells(fs->value + line[current].start,
					 fs->value + fs->state);

		new_pos(fs, line, current - 1, len);
	}

	if (old_state != fs->state ) {
		if (fs->state_cell && fs->state == line[current - 1].start) {
			unsigned char *new_value;

			new_value = utf8_prevchar(fs->value + fs->state, 1, fs->value);
			fs->state_cell = new_value - fs->value;
		} else
			fs->state_cell = 0;
	}

	return 0;
}

#else

static int
do_op_up(struct form_state *fs, struct line_info *line, int current)
{
	if (current == -1) return 0;
	if (!current) return 1;

	fs->state -= line[current].start - line[current-1].start;
	int_upper_bound(&fs->state, line[current-1].end);
	return 0;
}
#endif /* CONFIG_UTF8 */

#ifdef CONFIG_UTF8
static int
do_op_down(struct form_state *fs, struct line_info *line, int current, int utf8)
{
	int old_state;

	if (current == -1) return 0;

	if (line[current + 1 - !!fs->state_cell].start == -1) return 1;

	if (!utf8) {
		fs->state += line[current+1].start - line[current].start;
		int_upper_bound(&fs->state, line[current+1].end);
		return 0;
	}

	old_state = fs->state;
	if (fs->state_cell) {
		int len = utf8_ptr2cells(fs->value + line[current - 1].start,
			                 fs->value + fs->state_cell);

		new_pos(fs, line, current, len + line[current - 1].last_char_width);
	} else {
		int len = utf8_ptr2cells(fs->value + line[current].start,
					 fs->value + fs->state);

		new_pos(fs, line, current + 1, len);
	}
	if (old_state != fs->state ) {
		if (fs->state_cell && fs->state == line[current+1].start) {
			unsigned char *new_value;

			new_value = utf8_prevchar(fs->value + fs->state, 1, fs->value);
			fs->state_cell = new_value - fs->value;
		} else
			fs->state_cell = 0;
	}
	return 0;
}

#else

static int
do_op_down(struct form_state *fs, struct line_info *line, int current)
{
	if (current == -1) return 0;
	if (line[current+1].start == -1) return 1;

	fs->state += line[current+1].start - line[current].start;
	int_upper_bound(&fs->state, line[current+1].end);
	return 0;
}
#endif /* CONFIG_UTF8 */

#ifdef CONFIG_UTF8
static int
do_op_end(struct form_state *fs, struct line_info *line, int current, int utf8)
{
	if (current == -1) {
		fs->state = strlen(fs->value);
		return 0;
	}

	if (!utf8) {
		int wrap = line[current + 1].start == line[current].end;

		/* Don't jump to next line when wrapping. */
		fs->state = int_max(0, line[current].end - wrap);
		return 0;
	}

	current -= !!fs->state_cell;
	fs->state = line[current].end;
	if (line[current].split_next) {
		unsigned char *new_value;

		new_value = utf8_prevchar(fs->value + fs->state, 1, fs->value);
		fs->state_cell = new_value - fs->value;
	} else {
		fs->state_cell = 0;
	}
	return 0;
}

#else

static int
do_op_end(struct form_state *fs, struct line_info *line, int current)
{
	if (current == -1) {
		fs->state = strlen(fs->value);

	} else {
		int wrap = line[current + 1].start == line[current].end;

		/* Don't jump to next line when wrapping. */
		fs->state = int_max(0, line[current].end - wrap);
	}
	return 0;
}
#endif /* CONFIG_UTF8 */

static int
#ifdef CONFIG_UTF8
do_op_bob(struct form_state *fs, struct line_info *line, int current, int utf8)
#else
do_op_bob(struct form_state *fs, struct line_info *line, int current)
#endif /* CONFIG_UTF8 */
{
	if (current == -1) return 0;

	fs->state -= line[current].start;
	int_upper_bound(&fs->state, line[0].end);
	return 0;
}

static int
#ifdef CONFIG_UTF8
do_op_eob(struct form_state *fs, struct line_info *line, int current, int utf8)
#else
do_op_eob(struct form_state *fs, struct line_info *line, int current)
#endif /* CONFIG_UTF8 */
{
	if (current == -1) {
		fs->state = strlen(fs->value);

	} else {
		int last = get_textarea_line_number(line, strlen(fs->value));

		assertm(last != -1, "line info corrupt");

		fs->state += line[last].start - line[current].start;
		int_upper_bound(&fs->state, line[last].end);
	}
	return 0;
}

#ifdef CONFIG_UTF8
enum frame_event_status
textarea_op_home(struct form_state *fs, struct form_control *fc, int utf8)
{
	return textarea_op(fs, fc, utf8, do_op_home);
}
#else
enum frame_event_status
textarea_op_home(struct form_state *fs, struct form_control *fc)
{
	return textarea_op(fs, fc, do_op_home);
}
#endif /* CONFIG_UTF8 */

#ifdef CONFIG_UTF8
enum frame_event_status
textarea_op_up(struct form_state *fs, struct form_control *fc, int utf8)
{
	return textarea_op(fs, fc, utf8, do_op_up);
}
#else
enum frame_event_status
textarea_op_up(struct form_state *fs, struct form_control *fc)
{
	return textarea_op(fs, fc, do_op_up);
}
#endif /* CONFIG_UTF8 */

#ifdef CONFIG_UTF8
enum frame_event_status
textarea_op_down(struct form_state *fs, struct form_control *fc, int utf8)
{
	return textarea_op(fs, fc, utf8, do_op_down);
}
#else
enum frame_event_status
textarea_op_down(struct form_state *fs, struct form_control *fc)
{
	return textarea_op(fs, fc, do_op_down);
}
#endif /* CONFIG_UTF8 */

#ifdef CONFIG_UTF8
enum frame_event_status
textarea_op_end(struct form_state *fs, struct form_control *fc, int utf8)
{
	return textarea_op(fs, fc, utf8, do_op_end);
}
#else
enum frame_event_status
textarea_op_end(struct form_state *fs, struct form_control *fc)
{
	return textarea_op(fs, fc, do_op_end);
}
#endif /* CONFIG_UTF8 */

/* Set the form state so the cursor is on the first line of the buffer.
 * Preserve the column if possible. */
#ifdef CONFIG_UTF8
enum frame_event_status
textarea_op_bob(struct form_state *fs, struct form_control *fc, int utf8)
{
	return textarea_op(fs, fc, utf8, do_op_bob);
}
#else
enum frame_event_status
textarea_op_bob(struct form_state *fs, struct form_control *fc)
{
	return textarea_op(fs, fc, do_op_bob);
}
#endif /* CONFIG_UTF8 */

/** Set the form state so the cursor is on the last line of the buffer. Preserve
 * the column if possible. This is done by getting current and last line and
 * then shifting the state by the delta of both lines start position bounding
 * the whole thing to the end of the last line. */
#ifdef CONFIG_UTF8
enum frame_event_status
textarea_op_eob(struct form_state *fs, struct form_control *fc, int utf8)
{
	return textarea_op(fs, fc, utf8, do_op_eob);
}
#else
enum frame_event_status
textarea_op_eob(struct form_state *fs, struct form_control *fc)
{
	return textarea_op(fs, fc, do_op_eob);
}
#endif /* CONFIG_UTF8 */

enum frame_event_status
#ifdef CONFIG_UTF8
textarea_op_enter(struct form_state *fs, struct form_control *fc, int utf8)
#else
textarea_op_enter(struct form_state *fs, struct form_control *fc)
#endif /* CONFIG_UTF8 */
{
	assert(fs && fs->value && fc);
	if_assert_failed return FRAME_EVENT_OK;

	if (form_field_is_readonly(fc)
	    || strlen(fs->value) >= fc->maxlength
	    || !insert_in_string(&fs->value, fs->state, "\n", 1))
		return FRAME_EVENT_OK;

	fs->state++;
	return FRAME_EVENT_REFRESH;
}

#ifdef CONFIG_UTF8
static int
do_op_left(struct form_state *fs, struct line_info *line, int current, int utf8)
{
	int old_state;
	int new_state;
	unsigned char *new_value;

	if (!utf8) {
		fs->state = int_max(fs->state - 1, 0);
		return 0;
	}

	if (fs->state_cell) {
		fs->state = fs->state_cell;
		fs->state_cell = 0;
		return 0;
	}

	old_state = fs->state;
	new_value = utf8_prevchar(fs->value + fs->state, 1, fs->value);
	new_state = new_value - fs->value;

	if (old_state != new_state) {
		if (old_state == line[current].start && line[current].split_prev)
			fs->state_cell = new_state;
		else
			fs->state = new_state;
	}
	return 0;
}

static int
do_op_right(struct form_state *fs, struct line_info *line, int current, int utf8)
{
	unsigned char *text, *end;
	int old_state;

	if (!utf8) {
		/* TODO: zle */
		fs->state = int_min(fs->state + 1, strlen(fs->value));
		return 0;
	}

	if (fs->state_cell) {
		fs->state_cell = 0;
		return 0;
	}

	text = fs->value + fs->state;
	end = strchr(text, '\0');
	old_state = fs->state;
	utf8_to_unicode(&text, end);

	fs->state = text - fs->value;

	if (old_state != fs->state && line[current].split_next) {
		fs->state_cell = (fs->state == line[current].end) ? old_state:0;
	} else if (!line[current].split_next) {
		fs->state_cell = 0;
	}

	return 0;
}
#endif /* CONFIG_UTF8 */

#ifdef CONFIG_UTF8
enum frame_event_status
textarea_op_left(struct form_state *fs, struct form_control *fc, int utf8)
{
	return textarea_op(fs, fc, utf8, do_op_left);
}

enum frame_event_status
textarea_op_right(struct form_state *fs, struct form_control *fc, int utf8)
{
	return textarea_op(fs, fc, utf8, do_op_right);
}
#endif /* CONFIG_UTF8 */

void
set_textarea(struct document_view *doc_view, int direction)
{
	struct form_control *fc;
	struct form_state *fs;
	struct link *link;
#ifdef CONFIG_UTF8
	int utf8 = doc_view->document->options.utf8;
#endif /* CONFIG_UTF8 */

	assert(doc_view && doc_view->vs && doc_view->document);
	assert(direction == 1 || direction == -1);
	if_assert_failed return;

	link = get_current_link(doc_view);
	if (!link || link->type != LINK_AREA)
		return;

	fc = get_link_form_control(link);
	assertm(fc != NULL, "link has no form control");
	if_assert_failed return;

	if (fc->mode == FORM_MODE_DISABLED) return;

	fs = find_form_state(doc_view, fc);
	if (!fs || !fs->value) return;

	/* Depending on which way we entered the textarea move cursor so that
	 * it is available at end or start. */
#ifdef CONFIG_UTF8
	if (direction == 1)
		textarea_op_eob(fs, fc, utf8);
	else
		textarea_op_bob(fs, fc, utf8);
#else
	if (direction == 1)
		textarea_op_eob(fs, fc);
	else
		textarea_op_bob(fs, fc);
#endif /* CONFIG_UTF8 */
}
