/* $Id: search.h,v 1.21 2004/08/15 11:52:51 jonas Exp $ */

#ifndef EL__VIEWER_TEXT_SEARCH_H
#define EL__VIEWER_TEXT_SEARCH_H

#include "document/view.h"

struct session;
struct terminal;

void draw_searched(struct terminal *term, struct document_view *doc_view);

enum frame_event_status find_next(struct session *ses, struct document_view *doc_view, int direction);

enum frame_event_status search_dlg(struct session *ses, struct document_view *doc_view, int direction);
enum frame_event_status search_typeahead(struct session *ses, struct document_view *doc_view, int direction);

void init_search_history(void);
void done_search_history(void);

static inline int has_search_word(struct document_view *doc_view)
{
	return (doc_view->search_word
		&& *doc_view->search_word
		&& (*doc_view->search_word)[0]);
}

#endif
