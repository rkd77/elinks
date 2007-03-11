/* Generic support for edit/search historyitem/bookmark dialog */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "dialogs/edit.h"
#include "intl/gettext/libintl.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: Move to bfu/. It has no bussiness to do in dialogs/. --pasky */


static widget_handler_status_T
my_cancel_dialog(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	((void (*)(struct dialog *)) widget_data->widget->data)(dlg_data->dlg);
	return cancel_dialog(dlg_data, widget_data);
}


/* Edits an item's fields.
 * If parent is defined, then that points to a dialog that should be sent
 * an update when the add is done.
 *
 * If either of src_name or src_url are NULL, try to obtain the name and url
 * of the current document. If you want to create two null fields, pass in a
 * pointer to a zero length string (""). */
/* TODO: In bookmark/dialogs.c most users seem to want also the dialog_data
 * so we should make when_*() functions take dialog_data instead. --jonas */
void
do_edit_dialog(struct terminal *term, int intl, unsigned char *title,
	       const unsigned char *src_name,
	       const unsigned char *src_url,
	       struct session *ses, struct dialog_data *parent,
	       done_handler_T *when_done,
	       void when_cancel(struct dialog *),
	       void *done_data,
	       enum edit_dialog_type dialog_type)
{
	/* [gettext_accelerator_context(do_edit_dialog)] */
	unsigned char *name, *url;
	struct dialog *dlg;

	if (intl) title = _(title, term);

	/* Number of fields in edit dialog --Zas */
#define EDIT_WIDGETS_COUNT 5

	/* Create the dialog */
	dlg = calloc_dialog(EDIT_WIDGETS_COUNT, 2 * MAX_STR_LEN);
	if (!dlg) return;

	name = get_dialog_offset(dlg, EDIT_WIDGETS_COUNT);
	url = name + MAX_STR_LEN;

	/* Get the name */
	if (!src_name) {
		/* Unknown name. */
		get_current_title(ses, name, MAX_STR_LEN);
	} else {
		/* Known name. */
		safe_strncpy(name, src_name, MAX_STR_LEN);
	}

	/* Get the url */
	if (!src_url) {
		/* Unknown . */
		get_current_url(ses, url, MAX_STR_LEN);
	} else {
		/* Known url. */
		safe_strncpy(url, src_url, MAX_STR_LEN);
	}

	dlg->title = title;
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.maximize_width = 1;
	dlg->udata = parent;
	dlg->udata2 = done_data;

	if (dialog_type == EDIT_DLG_ADD)
		add_dlg_field(dlg, _("Name", term), 0, 0, check_nonempty, MAX_STR_LEN, name, NULL);
	else
		add_dlg_field(dlg, _("Name", term), 0, 0, NULL, MAX_STR_LEN, name, NULL);

	add_dlg_field(dlg, _("URL", term), 0, 0, NULL, MAX_STR_LEN, url, NULL);

	add_dlg_ok_button(dlg, _("~OK", term), B_ENTER, when_done, dlg);
	add_dlg_button(dlg, _("C~lear", term), 0, clear_dialog, NULL);

	if (when_cancel)
		add_dlg_button(dlg, _("~Cancel", term), B_ESC,
			       my_cancel_dialog, when_cancel);
	else
		add_dlg_button(dlg, _("~Cancel", term), B_ESC,
			       cancel_dialog, NULL);

	add_dlg_end(dlg, EDIT_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, (void *) NULL));

#undef EDIT_WIDGETS_COUNT
}
