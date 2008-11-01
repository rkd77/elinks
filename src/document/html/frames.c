/* HTML frames parser */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "document/document.h"
#include "document/html/frames.h"
#include "document/options.h"
#include "document/renderer.h"
#include "document/view.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/string.h"
#include "util/time.h"


void
add_frameset_entry(struct frameset_desc *frameset_desc,
		   struct frameset_desc *subframe,
		   unsigned char *name, unsigned char *url)
{
	struct frame_desc *frame_desc;
	int offset;

	assert(frameset_desc);
	if_assert_failed return;

	/* FIXME: The following is triggered by
	 * http://www.thegoodlookingorganisation.co.uk/main.htm.
	 * There may exist a true fix for this... --Zas */
	/* May the one truly fixing this notify'n'close bug 237 in the
	 * Bugzilla... --pasky */
	if (frameset_desc->box.y >= frameset_desc->box.height)
		return;

	offset = frameset_desc->box.x
		 + frameset_desc->box.y * frameset_desc->box.width;
	frame_desc = &frameset_desc->frame_desc[offset];
	frame_desc->subframe = subframe;
	frame_desc->name = null_or_stracpy(name);
	frame_desc->uri = (url && *url) ? get_uri(url, 0) : NULL;
	if (!frame_desc->uri)
		frame_desc->uri = get_uri("about:blank", 0);

	frameset_desc->box.x++;
	if (frameset_desc->box.x >= frameset_desc->box.width) {
		frameset_desc->box.x = 0;
		frameset_desc->box.y++;
		/* FIXME: check y here ? --Zas */
	}
}

struct frameset_desc *
create_frameset(struct frameset_param *fp)
{
	struct frameset_desc *fd;
	unsigned int size;

	assert(fp);
	if_assert_failed return NULL;

	assertm(fp->x > 0 && fp->y > 0,
		"Bad size of frameset: x=%d y=%d", fp->x, fp->y);
	if_assert_failed {
		if (fp->x <= 0) fp->x = 1;
		if (fp->y <= 0) fp->y = 1;
	}

	size = fp->x * fp->y;
	/* size - 1 since one struct frame_desc is already reserved
	 * in struct frameset_desc. */
	fd = mem_calloc(1, sizeof(*fd)
			   + (size - 1) * sizeof(fd->frame_desc[0]));
	if (!fd) return NULL;

	{
		int i;

		for (i = 0; i < size; i++) {
			fd->frame_desc[i].width = fp->width[i % fp->x];
			fd->frame_desc[i].height = fp->height[i / fp->x];
		}
	}

	fd->n = size;
	fd->box.width = fp->x;
	fd->box.height = fp->y;

	if (fp->parent)
		add_frameset_entry(fp->parent, fd, NULL, NULL);

	return fd;
}

static void
add_frame_to_list(struct session *ses, struct document_view *doc_view)
{
	struct document_view *ses_doc_view;

	assert(ses && doc_view);
	if_assert_failed return;

	foreach (ses_doc_view, ses->scrn_frames) {
		int sx = ses_doc_view->box.x;
		int sy = ses_doc_view->box.y;
		int x = doc_view->box.x;
		int y = doc_view->box.y;

		if (sy > y || (sy == y && sx > x)) {
			add_at_pos(ses_doc_view->prev, doc_view);
			return;
		}
	}

	add_to_list_end(ses->scrn_frames, doc_view);
}

static struct document_view *
find_fd(struct session *ses, unsigned char *name,
	int depth, int x, int y)
{
	struct document_view *doc_view;

	assert(ses && name);
	if_assert_failed return NULL;

	foreachback (doc_view, ses->scrn_frames) {
		if (doc_view->used) continue;
		if (c_strcasecmp(doc_view->name, name)) continue;

		doc_view->used = 1;
		doc_view->depth = depth;
		return doc_view;
	}

	doc_view = mem_calloc(1, sizeof(*doc_view));
	if (!doc_view) return NULL;

	doc_view->used = 1;
	doc_view->name = stracpy(name);
	if (!doc_view->name) {
		mem_free(doc_view);
		return NULL;
	}
	doc_view->depth = depth;
	doc_view->session = ses;
	doc_view->search_word = &ses->search_word;
	set_box(&doc_view->box, x, y, 0, 0);

	add_frame_to_list(ses, doc_view);

	return doc_view;
}

static struct document_view *
format_frame(struct session *ses, struct frame_desc *frame_desc,
	     struct document_options *o, int depth)
{
	struct view_state *vs;
	struct document_view *doc_view;
	int plain;

	assert(ses && frame_desc && o);
	if_assert_failed return NULL;

	while (1) {
		struct cache_entry *cached;
		struct frame *frame = ses_find_frame(ses, frame_desc->name);

		if (!frame) return NULL;

		vs = &frame->vs;
		cached = find_in_cache(vs->uri);
		if (!cached) return NULL;

		plain = o->plain;
		if (vs->plain != -1) o->plain = vs->plain;

		if (!cached->redirect || frame->redirect_cnt >= MAX_REDIRECTS)
			break;

		frame->redirect_cnt++;
		done_uri(vs->uri);
		vs->uri = get_uri_reference(cached->redirect);
#ifdef CONFIG_ECMASCRIPT
		vs->ecmascript_fragile = 1;
#endif
		o->plain = plain;
	}

	doc_view = find_fd(ses, frame_desc->name, depth, o->box.x, o->box.y);
	if (doc_view) {
		render_document(vs, doc_view, o);
		assert(doc_view->document);
		doc_view->document->frame = frame_desc;
	}
	o->plain = plain;

	return doc_view;
}

void
format_frames(struct session *ses, struct frameset_desc *fsd,
	      struct document_options *op, int depth)
{
	struct document_options o;
	int j, n;

	assert(ses && fsd && op);
	if_assert_failed return;

	if (depth > HTML_MAX_FRAME_DEPTH) {
		return;
	}

	copy_struct(&o, op);

	o.margin = !!o.margin;

	n = 0;
	for (j = 0; j < fsd->box.height; j++) {
		int i;

		o.box.x = op->box.x;
		for (i = 0; i < fsd->box.width; i++) {
			struct frame_desc *frame_desc = &fsd->frame_desc[n];

			o.box.width = frame_desc->width;
			o.box.height = frame_desc->height;
			o.framename = frame_desc->name;
			if (frame_desc->subframe) {
				format_frames(ses, frame_desc->subframe, &o, depth + 1);
			} else if (frame_desc->name) {
				struct document_view *doc_view;

				doc_view = format_frame(ses, frame_desc, &o, depth);
#ifdef CONFIG_DEBUG
				do_not_optimize_here(&doc_view);
#endif
				if (doc_view && document_has_frames(doc_view->document)) {
					struct document *document = doc_view->document;

					/* Make sure the frameset does not
					 * disappear during processing further
					 * frames - this can happen when we
					 * kick the same-frame-ids bug and
					 * replace view_state of an existing
					 * frame - if that frame is *this* one,
					 * it gets detached, losing the
					 * document and if we aren't lucky the
					 * document gets burned in
					 * shrink_format_cache(). Also note
					 * that the same bug replacing
					 * view_state also causes doc_view
					 * recyclation so we end up with a
					 * *different* document attached to it
					 * after the call, so we need to cache
					 * it. It's all fun to debug, really.
					 * --pasky */
					object_lock(document);
					format_frames(ses, document->frame_desc,
						      &o, depth + 1);
					object_unlock(document);
				}
			}
			o.box.x += o.box.width + 1;
			n++;
#ifdef CONFIG_DEBUG
			/* This makes this ugly loop actually at least remotely
			 * debuggable by gdb, otherwise the compiler happily
			 * randomly trashes or just blasts away *all* of these
			 * variables. */
			do_not_optimize_here(&n);
			do_not_optimize_here(&i);
			do_not_optimize_here(&j);
			do_not_optimize_here(&fsd);
			do_not_optimize_here(&frame_desc);
#endif
		}
		o.box.y += o.box.height + 1;
	}
}


/* Frame width computation tools */

/* Returns 0 on error. */
static int
distribute_rows_or_cols(int *val_, int max_value, int *values, int values_count)
{
	int i;
	int divisor = 0;
	int tmp_val;
	int val = *val_ - max_value;

	for (i = 0; i < values_count; i++) {
		if (values[i] < 1)
			values[i] = 1;

		divisor += values[i];
	}

	assert(divisor);

	tmp_val = val;
	for (i = 0; i < values_count; i++) {
		int tmp;

		/* SIGH! gcc 2.7.2.* has an optimizer bug! */
		do_not_optimize_here_gcc_2_7(&divisor);
		tmp = values[i] * (divisor - tmp_val) / divisor;
		val -= values[i] - tmp;
		values[i] = tmp;
	}

	while (val) {
		int flag = 0;

		for (i = 0; i < values_count; i++) {
			if (val < 0) values[i]++, val++, flag = 1;
			if (val > 0 && values[i] > 1) values[i]--, val--, flag = 1;
			if (!val) break;
		}
		if (!flag) break;
	}

	*val_ = val;
	return 1;
}

/* Returns 0 on error. */
static int
distribute_rows_or_cols_that_left(int *val_, int max_value, int *values, int values_count)
{
	int i;
	int val = *val_;
	int *tmp_values;
	int divisor = 0;
	int tmp_val;

	tmp_values = fmem_alloc(values_count * sizeof(*tmp_values));
	if (!tmp_values) return 0;
	memcpy(tmp_values, values, values_count * sizeof(*tmp_values));

	for (i = 0; i < values_count; i++)
		if (values[i] < 1)
			values[i] = 1;
	val = max_value - val;

	for (i = 0; i < values_count; i++)
		if (tmp_values[i] < 0)
			divisor += -tmp_values[i];
	assert(divisor);

	tmp_val = val;
	for (i = 0; i < values_count; i++)
		if (tmp_values[i] < 0) {
			int tmp = (-tmp_values[i] * tmp_val / divisor);

			values[i] += tmp;
			val -= tmp;
		}
	assertm(val >= 0, "distribute_rows_or_cols_that_left: val < 0");
	if_assert_failed val = 0;

	for (i = 0; i < values_count; i++)
		if (tmp_values[i] < 0 && val)
			values[i]++, val--;

	assertm(val <= 0, "distribute_rows_or_cols_that_left: val > 0");
	if_assert_failed val = 0;

	fmem_free(tmp_values);

	*val_ = val;
	return 1;
}

/* Returns 0 on error. */
static int
extract_rows_or_cols_values(unsigned char *str, int max_value, int pixels_per_char,
			    int **new_values, int *new_values_count)
{
	unsigned char *tmp_str;
	int *values = NULL;
	int values_count = 0;

	while (1) {
		unsigned char *end = str;
		int *tmp_values;
		unsigned long number;
		int val = -1;	/* Wildcard */

		skip_space(str);

		/* Some platforms (FreeBSD) set errno when the first char is
		 * not a digit others (GNU/Linux) don't so ignore errno. */
		/* Extract number. */
		number = strtoul(str, (char **) &end, 10);
		if (end == str) {
			number = 0;
		} else {
			str = end;
		}

		/* @number is an ulong, but @val is int,
		 * so check if @number is in a reasonable
		 * range to prevent bad things. */
		if (number <= 0xffff) {
			if (*str == '%')	/* Percentage */
				val = int_min((int) number, 100) * max_value / 100;
			else if (*str != '*')	/* Pixels */
				val = (number + (pixels_per_char - 1) / 2) / pixels_per_char;
			else if (number)	/* Fraction, marked by negative value. */
				val = -number;
		}

		/* Save value. */
		tmp_values = mem_realloc(values, (values_count + 1) * sizeof(*tmp_values));
		if (!tmp_values) return 0;

		values = tmp_values;
		values[values_count++] = val;

		/* Check for next field if any. */
		tmp_str = strchr(str, ',');
		if (!tmp_str) break;	/* It was the last field. */

		str = tmp_str + 1;
	}

	*new_values = values;
	*new_values_count = values_count;

	return 1;
}

/* Parse rows and cols attribute values and calculate appropriated values for display.
 * It handles things like:
 * <frameset cols="140,260,160">			values in pixels
 * <frameset cols="1*,2*,3*"> 				values in fractions
 * <frameset cols="320,*">				wildcard
 * <frameset cols="33%,33%,33%" rows="33%,33%,33%"> 	values in percentage
 * */
void
parse_frame_widths(unsigned char *str, int max_value, int pixels_per_char,
		   int **new_values, int *new_values_count)
{
	int val, ret;
	int *values;
	int values_count;
	int i;

	*new_values_count = 0;

	ret = extract_rows_or_cols_values(str, max_value, pixels_per_char,
					  &values, &values_count);
	if (!ret) return;

	/* Here begins distribution between rows or cols. */

	val = 2 * values_count - 1;
	for (i = 0; i < values_count; i++)
		if (values[i] > 0)
			val += values[i] - 1;

	if (val >= max_value) {
		ret = distribute_rows_or_cols(&val, max_value, values, values_count);
	} else {
		int neg = 0;

		for (i = 0; i < values_count; i++)
			if (values[i] < 0)
				neg = 1;

		if (neg)
			ret = distribute_rows_or_cols_that_left(&val, max_value, values, values_count);
		else
			ret = distribute_rows_or_cols(&val, max_value, values, values_count);
	}

	if (!ret) return;

	for (i = 0; i < values_count; i++)
		if (!values[i]) {
			int j;
			int maxval = 0;
			int maxpos = 0;

			for (j = 0; j < values_count; j++)
				if (values[j] > maxval)
					maxval = values[j], maxpos = j;
			if (maxval)
				values[i] = 1, values[maxpos]--;
		}

	*new_values = values;
	*new_values_count = values_count;
}
