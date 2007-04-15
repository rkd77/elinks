/* Form history related dialogs */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "formhist/dialogs.h"
#include "formhist/formhist.h"
#include "dialogs/edit.h"
#include "document/forms.h"
#include "intl/gettext/libintl.h"
#include "main/object.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"


static void
lock_formhist_data(struct listbox_item *item)
{
	object_lock((struct formhist_data *) item->udata);
}

static void
unlock_formhist_data(struct listbox_item *item)
{
	object_unlock((struct formhist_data *) item->udata);
}

static int
is_formhist_data_used(struct listbox_item *item)
{
	return is_object_used((struct formhist_data *) item->udata);
}

static unsigned char *
get_formhist_data_text(struct listbox_item *item, struct terminal *term)
{
	struct formhist_data *formhist_data = item->udata;

	return stracpy(formhist_data->url);
}

static unsigned char *
get_formhist_data_info(struct listbox_item *item, struct terminal *term)
{
	struct formhist_data *formhist_data = item->udata;
	struct string info;
	struct submitted_value *sv;

	if (item->type == BI_FOLDER) return NULL;
	if (!init_string(&info)) return NULL;

	add_format_to_string(&info, "%s: %s", _("URL", term), formhist_data->url);
	add_char_to_string(&info, '\n');

	if (formhist_data->dontsave)
		add_to_string(&info, _("Forms are never saved for this URL.", term));
	else
		add_to_string(&info, _("Forms are saved for this URL.", term));

	add_char_to_string(&info, '\n');
	foreach (sv, *formhist_data->submit) {
		add_format_to_string(&info, "\n[%8s] ", form_type2str(sv->type));

		add_to_string(&info, sv->name);
		add_to_string(&info, " = ");
		if (sv->value && *sv->value) {
			if (sv->type != FC_PASSWORD)
				add_to_string(&info, sv->value);
			else
				add_to_string(&info, "********");
		}
	}

	return info.source;
}

static struct uri *
get_formhist_data_uri(struct listbox_item *item)
{
	struct formhist_data *formhist_data = item->udata;

	return get_uri(formhist_data->url, 0);
}

static struct listbox_item *
get_formhist_data_root(struct listbox_item *item)
{
	return NULL;
}

static int
can_delete_formhist_data(struct listbox_item *item)
{
	return 1;
}

static void
delete_formhist_data(struct listbox_item *item, int last)
{
	struct formhist_data *formhist_data = item->udata;

	assert(!is_object_used(formhist_data));

	delete_formhist_item(formhist_data);
}

static struct listbox_ops_messages formhist_messages = {
	/* cant_delete_item */
	N_("Sorry, but form \"%s\" cannot be deleted."),
	/* cant_delete_used_item */
	N_("Sorry, but form \"%s\" is being used by something else."),
	/* cant_delete_folder */
	NULL,
	/* cant_delete_used_folder */
	NULL,
	/* delete_marked_items_title */
	N_("Delete marked forms"),
	/* delete_marked_items */
	N_("Delete marked forms?"),
	/* delete_folder_title */
	NULL,
	/* delete_folder */
	NULL,
	/* delete_item_title */
	N_("Delete form"),
	/* delete_item; xgettext:c-format */
	N_("Delete this form?"),
	/* clear_all_items_title */
	N_("Clear all forms"),
	/* clear_all_items_title */
	N_("Do you really want to remove all forms?"),
};

static const struct listbox_ops formhist_listbox_ops = {
	lock_formhist_data,
	unlock_formhist_data,
	is_formhist_data_used,
	get_formhist_data_text,
	get_formhist_data_info,
	get_formhist_data_uri,
	get_formhist_data_root,
	NULL,
	can_delete_formhist_data,
	delete_formhist_data,
	NULL,
	&formhist_messages,
};

static widget_handler_status_T
push_login_button(struct dialog_data *dlg_data,
		  struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct formhist_data *formhist_data;
	struct terminal *term = dlg_data->win->term;

	if (!box->sel || !box->sel->udata) return EVENT_PROCESSED;

	formhist_data = box->sel->udata;

	if (formhist_data->dontsave) {
		info_box(term, 0, N_("Form not saved"), ALIGN_CENTER,
			 N_("No saved information for this URL.\n"
			 "If you want to save passwords for this URL, enable "
			 "it by pressing the \"Toggle saving\" button."));
		return EVENT_PROCESSED;
	}

	push_hierbox_goto_button(dlg_data, button);

	return EVENT_PROCESSED;
}

static widget_handler_status_T
push_toggle_dontsave_button(struct dialog_data *dlg_data,
			    struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct formhist_data *formhist_data;

	if (!box->sel || !box->sel->udata) return EVENT_PROCESSED;

	formhist_data = box->sel->udata;

	formhist_data->dontsave = !formhist_data->dontsave;
	return EVENT_PROCESSED;
}

static widget_handler_status_T
push_save_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	save_formhist_to_file();
	return EVENT_PROCESSED;
}

static const struct hierbox_browser_button formhist_buttons[] = {
	/* [gettext_accelerator_context(.formhist_buttons)] */
	{ N_("~Login"),         push_login_button,           1 },
	{ N_("~Info"),          push_hierbox_info_button,    1 },
	{ N_("~Delete"),        push_hierbox_delete_button,  1 },
	{ N_("~Toggle saving"), push_toggle_dontsave_button, 0 },
	{ N_("Clea~r"),         push_hierbox_clear_button,   1 },
	{ N_("Sa~ve"),          push_save_button,            0 },
};

struct_hierbox_browser(
	formhist_browser,
	N_("Form history manager"),
	formhist_buttons,
	&formhist_listbox_ops
);

void
formhist_manager(struct session *ses)
{
	load_formhist_from_file();
	hierbox_browser(&formhist_browser, ses);
}
