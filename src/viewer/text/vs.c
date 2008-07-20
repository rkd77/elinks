/** View state manager
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "protocol/uri.h"
#include "session/location.h"
#include "session/session.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


/** @relates view_state */
void
init_vs(struct view_state *vs, struct uri *uri, int plain)
{
	memset(vs, 0, sizeof(*vs));
	vs->current_link = -1;
	vs->old_current_link = -1;
	vs->plain = plain;
	vs->uri = uri ? get_uri_reference(uri) : NULL;
	vs->did_fragment = !uri->fragmentlen;
#ifdef CONFIG_ECMASCRIPT
	/* If we ever get to render this vs, give it an interpreter. */
	vs->ecmascript_fragile = 1;
#endif
	init_list(vs->forms);
}

/** @relates view_state */
void
destroy_vs(struct view_state *vs, int blast_ecmascript)
{
	struct form_view *fv, *next;

	/* form_state contains a pointer to form_view, so it's safest
	 * to delete the form_state first.  */
	for (; vs->form_info_len > 0; vs->form_info_len--)
		done_form_state(&vs->form_info[vs->form_info_len - 1]);
	mem_free_set(&vs->form_info, NULL);

	foreachsafe (fv, next, vs->forms) {
		del_from_list(fv);
		done_form_view(fv);
	}
	
	if (vs->uri) done_uri(vs->uri);
#ifdef CONFIG_ECMASCRIPT
	if (blast_ecmascript && vs->ecmascript)
		ecmascript_put_interpreter(vs->ecmascript);
#endif
	if (vs->doc_view) {
		vs->doc_view->vs = NULL;
		vs->doc_view = NULL;
	}
}

/** @relates view_state */
void
copy_vs(struct view_state *dst, struct view_state *src)
{
	struct form_view *fv;

	copy_struct(dst, src);

	/* We do not copy ecmascript stuff around since it's specific for
	 * a single location, offsprings (followups and so) nedd their own. */
#ifdef CONFIG_ECMASCRIPT
	dst->ecmascript = NULL;
	/* If we ever get to render this vs, give it an interpreter. */
	dst->ecmascript_fragile = 1;
#endif

	/* destroy_vs(vs) does mem_free_if(vs->form_info), so each
	 * view_state must have its own form_info.  Normally we make a
	 * copy below, but not if src->form_info_len is 0, which it
	 * can be even if src->form_info is not NULL.  */
	dst->form_info = NULL;

	/* Clean as a baby. */
	dst->doc_view = NULL;

	dst->uri = get_uri_reference(src->uri);
	/* Redo fragment if there is one? */
	dst->did_fragment = !src->uri->fragmentlen;

	init_list(dst->forms);
	foreach (fv, src->forms) {
		struct form_view *newfv = mem_calloc(1, sizeof(*newfv));

		if (!newfv) continue;
		newfv->form_num = fv->form_num;
		/* We do leave out the ECMAScript object intentionally. */
		add_to_list(dst->forms, newfv);
	}

	if (src->form_info_len) {
		dst->form_info = mem_alloc(src->form_info_len
					   * sizeof(*src->form_info));
		if (dst->form_info) {
			int i;

			memcpy(dst->form_info, src->form_info,
			       src->form_info_len * sizeof(*src->form_info));
			for (i = 0; i < src->form_info_len; i++) {
				struct form_state *srcfs = &src->form_info[i];
				struct form_state *dstfs = &dst->form_info[i];

#ifdef CONFIG_ECMASCRIPT
				dstfs->ecmascript_obj = NULL;
#endif
				if (srcfs->value)
					dstfs->value = stracpy(srcfs->value);
				/* XXX: This makes it O(nm). */
				dstfs->form_view =
					srcfs->form_view
					? find_form_view_in_vs(dst, srcfs->form_view->form_num)
					: NULL;
			}
		}
	}
}

void
check_vs(struct document_view *doc_view)
{
	struct view_state *vs = doc_view->vs;

	int_upper_bound(&vs->current_link, doc_view->document->nlinks - 1);

	if (vs->current_link != -1) {
		if (!current_link_is_visible(doc_view)) {
			struct link *links = doc_view->document->links;

			set_pos_x(doc_view, &links[vs->current_link]);
			set_pos_y(doc_view, &links[vs->current_link]);
		}
	} else {
		find_link_page_down(doc_view);
	}
}

void
next_frame(struct session *ses, int p)
{
	struct view_state *vs;
	struct document_view *doc_view;
	int n;

	if (!have_location(ses)
	    || (ses->doc_view && !document_has_frames(ses->doc_view->document)))
		return;

	ses->navigate_mode = NAVIGATE_LINKWISE;

	vs = &cur_loc(ses)->vs;

	n = 0;
	foreach (doc_view, ses->scrn_frames) {
		if (!document_has_frames(doc_view->document))
			n++;
	}

	vs->current_link += p;
	if (!n) n = 1;
	while (vs->current_link < 0) vs->current_link += n;
	vs->current_link %= n;
}
