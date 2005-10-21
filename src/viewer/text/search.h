
#ifndef EL__VIEWER_TEXT_SEARCH_H
#define EL__VIEWER_TEXT_SEARCH_H

#include "config/kbdbind.h"
#include "document/view.h"
#include "viewer/action.h"

struct module;
struct session;
struct terminal;

extern struct module search_history_module;

void draw_searched(struct terminal *term, struct document_view *doc_view);

enum frame_event_status find_next(struct session *ses, struct document_view *doc_view, int direction);

enum frame_event_status search_dlg(struct session *ses, struct document_view *doc_view, int direction);
enum frame_event_status search_typeahead(struct session *ses, struct document_view *doc_view, action_id_T action_id);

static inline int has_search_word(struct document_view *doc_view)
{
	return (doc_view->search_word
		&& *doc_view->search_word
		&& (*doc_view->search_word)[0]);
}

#endif
