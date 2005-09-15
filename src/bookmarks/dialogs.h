/* $Id: dialogs.h,v 1.14 2004/07/15 00:45:33 jonas Exp $ */

#ifndef EL__BOOKMARKS_DIALOGS_H
#define EL__BOOKMARKS_DIALOGS_H

#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "terminal/terminal.h"
#include "sched/session.h"

/* Search memorization */
extern unsigned char *bm_last_searched_name;
extern unsigned char *bm_last_searched_url;
extern struct hierbox_browser bookmark_browser;

/* Launch the bookmark manager */
void bookmark_manager(struct session *ses);

/* Launch 'Add bookmark' dialog... */

/* ...with the given title and URL */
void launch_bm_add_dialog(struct terminal *term,
			  struct dialog_data *parent,
			  struct session *ses,
			  unsigned char *title,
			  unsigned char *url);

/* ...with the current document's title and URL */
void launch_bm_add_doc_dialog(struct terminal *term,
			      struct dialog_data *parent,
			      struct session *ses);

/* ...with the selected link's title and URL */
void launch_bm_add_link_dialog(struct terminal *term,
			       struct dialog_data *parent,
			       struct session *ses);

void bookmark_terminal_tabs_dialog(struct terminal *term);

#endif
