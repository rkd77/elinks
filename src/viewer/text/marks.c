/** Marks registry
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "protocol/uri.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/marks.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


/* TODO list:
 *
 * * Make it possible to go at marks which are set in a different document than
 * the active one. This will basically need some clever hacking in the
 * KP_MARK_GOTO handler, which first needs to load the URL and *then* jump to
 * the exact location and just overally restore the rest of view_state. Perhaps
 * this could also be somehow nicely unified with session.goto_position? --pasky
 *
 * * The lower-case chars should have per-document scope, while the upper-case
 * chars would have per-ELinks (over all instances as well) scope. The l-c marks
 * should be stored either in {struct document} or {struct location}, that needs
 * further investigation. Disappearing when document gets out of all caches
 * and/or histories is not an issue from my POV. However, it must be ensured
 * that all instances of the document (and only the document) share the same
 * marks. If we are dealing with frames, I mean content of one frame by
 * 'document' - each frame in the frameset gets own set of marks. --pasky
 *
 * * Number marks for last ten documents in session history. XXX: To be
 * meaningful, it needs to support last n both history and unhistory items.
 * --pasky
 *
 * * "''" support (jump to the last visited mark). (What if it was per-document
 * mark and we are already in another document now?) --pasky
 *
 * * When pressing "'", list of already set marks should appear. The next
 * pressed char, if letter, would still directly get stuff from the list.
 * --pasky */


/** Number of possible mark characters: upper-case and lower-case
 * ASCII letters.  The ::marks array is indexed by ASCII code of the
 * mark. */
#define MARKS_SIZE 26 * 2
static struct view_state *marks[MARKS_SIZE];

#define is_valid_mark_char(c)	isasciialpha(c)
#define is_valid_mark_index(i)  ((i) >= 0 && (i) < MARKS_SIZE)

static inline int
index_from_char(unsigned char mark)
{
	assert(is_valid_mark_char(mark));

	if (mark >= 'A' && mark <= 'Z')
		return mark - 'A';

	return mark - 'a' + 26;
}

void
goto_mark(unsigned char mark, struct view_state *vs)
{
	int old_current_link;
#ifdef CONFIG_ECMASCRIPT
	struct ecmascript_interpreter *ecmascript;
	int ecmascript_fragile;
#endif
	struct document_view *doc_view;
	int i;

	if (!is_valid_mark_char(mark))
		return;

	i = index_from_char(mark);
	assert(is_valid_mark_index(i));

	/* TODO: Support for cross-document marks. --pasky */
	if (!marks[i] || !compare_uri(marks[i]->uri, vs->uri, 0))
		return;

	old_current_link = vs->current_link;
#ifdef CONFIG_ECMASCRIPT
	ecmascript = vs->ecmascript;
	ecmascript_fragile = vs->ecmascript_fragile;
#endif
	doc_view = vs->doc_view;

	destroy_vs(vs, 0);
	copy_vs(vs, marks[i]);

	vs->doc_view = doc_view;
	vs->doc_view->vs = vs;
#ifdef CONFIG_ECMASCRIPT
	vs->ecmascript = ecmascript;
	vs->ecmascript_fragile = ecmascript_fragile;
#endif
	vs->old_current_link = old_current_link;
}

static void
free_mark_by_index(int i)
{
	assert(is_valid_mark_index(i));

	if (!marks[i]) return;

	destroy_vs(marks[i], 1);
	mem_free_set(&marks[i], NULL);
}

void
set_mark(unsigned char mark, struct view_state *mark_vs)
{
	struct view_state *vs;
	int i;

	if (!is_valid_mark_char(mark))
		return;

	i = index_from_char(mark);
	free_mark_by_index(i);

	if (!mark_vs) return;

	vs = mem_calloc(1, sizeof(*vs));
	if (!vs) return;
	copy_vs(vs, mark_vs);

	marks[i] = vs;
}

static void
done_marks(struct module *xxx)
{
	int i;

	for (i = 0; i < MARKS_SIZE; i++) {
		free_mark_by_index(i);
	}
}

struct module viewer_marks_module = struct_module(
	/* name: */		N_("Marks"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		done_marks
);
