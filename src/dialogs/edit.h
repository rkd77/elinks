#ifndef EL__DIALOGS_EDIT_H
#define EL__DIALOGS_EDIT_H

#include "bfu/dialog.h"
#include "session/session.h"
#include "terminal/terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

enum edit_dialog_type {
	EDIT_DLG_SEARCH,	/* search dialog */
	EDIT_DLG_ADD		/* edit/add dialog */
};

void do_edit_dialog(struct terminal *, int, char *,
		    const char *, const char *,
		    struct session *, struct dialog_data *,
		    done_handler_T *when_done,
		    void when_cancel(struct dialog *),
		    void *, enum edit_dialog_type);

#ifdef __cplusplus
}
#endif

#endif
