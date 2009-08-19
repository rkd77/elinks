/* Options dialogs */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* XXX: we _WANT_ strcasestr() ! */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/conf.h"
#include "config/dialogs.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "intl/gettext/libintl.h"
#include "main/event.h"
#include "main/object.h"
#include "session/session.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/secsave.h"


static void
disable_success_msgbox(void *dummy)
{
	get_opt_bool("ui.success_msgbox") = 0;
	option_changed(NULL, get_opt_rec(config_options, "ui.success_msgbox"));
}

void
write_config_dialog(struct terminal *term, unsigned char *config_file,
		    int secsave_error, int stdio_error)
{
	/* [gettext_accelerator_context(write_config_dialog)] */
	unsigned char *errmsg = NULL;
	unsigned char *strerr;

	if (secsave_error == SS_ERR_NONE && !stdio_error) {
		if (!get_opt_bool("ui.success_msgbox")) return;

		msg_box(term, NULL, MSGBOX_FREE_TEXT,
			N_("Write config success"), ALIGN_CENTER,
			msg_text(term, N_("Options were saved successfully to config file %s."),
				 config_file),
			NULL, 2,
			MSG_BOX_BUTTON(N_("~OK"), NULL, B_ENTER | B_ESC),
			MSG_BOX_BUTTON(N_("~Do not show anymore"), disable_success_msgbox, 0));
		return;
	}

	strerr = secsave_strerror(secsave_error, term);

	if (stdio_error > 0)
		errmsg = straconcat(strerr, " (", strerror(stdio_error), ")",
				    (unsigned char *) NULL);

	info_box(term, MSGBOX_FREE_TEXT,
		 N_("Write config error"), ALIGN_CENTER,
		 msg_text(term, N_("Unable to write to config file %s.\n%s"),
		 	  config_file, errmsg ? errmsg : strerr));

	mem_free_if(errmsg);
}



/****************************************************************************
  Option manager stuff.
****************************************************************************/

/* Implementation of the listbox operations */

static void
lock_option(struct listbox_item *item)
{
	object_lock((struct option *) item->udata);
}

static void
unlock_option(struct listbox_item *item)
{
	object_unlock((struct option *) item->udata);
}

static int
is_option_used(struct listbox_item *item)
{
	return is_object_used((struct option *) item->udata);
}

static unsigned char *
get_range_string(struct option *option)
{
	struct string info;

	if (!init_string(&info)) return NULL;

	if (option->type == OPT_BOOL)
		add_to_string(&info, "[0|1]");
	else if (option->type == OPT_INT || option->type == OPT_LONG)
		add_format_to_string(&info, "[%li..%li]", option->min, option->max);

	return info.source;
}

static unsigned char *
get_option_text(struct listbox_item *item, struct terminal *term)
{
	struct option *option = item->udata;
	unsigned char *desc = option->capt ? option->capt : option->name;

	if (option->flags & OPT_TOUCHED)
		return straconcat(_(desc, term),
				  " (", _("modified", term), ")",
				  (unsigned char *) NULL);

	return stracpy(_(desc, term));
}

static unsigned char *
get_option_info(struct listbox_item *item, struct terminal *term)
{
	struct option *option = item->udata;
	unsigned char *desc, *type;
	struct string info;

	if (!init_string(&info)) return NULL;

	add_format_to_string(&info, "%s: %s", _("Name", term), option->name);

	type = _(option_types[option->type].name, term);
	if (option->type == OPT_TREE) {
		type = straconcat(type, " ",
				  _("(expand by pressing space)", term),
				  (unsigned char *) NULL);
	}

	add_format_to_string(&info, "\n%s: %s", _("Type", term), type);

	if (option->type == OPT_TREE) {
		mem_free(type);
	}

	if (option_types[option->type].write) {
		unsigned char *range;
		struct string value;

		if (!init_string(&value)) {
			done_string(&info);
			return NULL;
		}

		option_types[option->type].write(option, &value);

		range = get_range_string(option);
		if (range) {
			if (*range) {
				add_to_string(&info, " ");
				add_to_string(&info, range);
			}
			mem_free(range);
		}
		add_format_to_string(&info, "\n%s: %s", _("Value", term), value.source);
		done_string(&value);

		if (option->flags & OPT_TOUCHED)
			add_to_string(&info, _("\n\nThis value has been changed"
					     " since you last saved your"
					     " configuration.", term));

	}

	desc = _(option->desc  ? option->desc : (unsigned char *) "N/A", term);
	if (*desc)
		add_format_to_string(&info, "\n\n%s:\n%s", _("Description", term), desc);

	return info.source;
}

static struct listbox_item *
get_option_root(struct listbox_item *item)
{
	struct option *option = item->udata;

	/* The config_options root has no listbox so return that
	 * we are at the bottom. */
	if (option->root == config_options) return NULL;

	return option->root ? option->root->box_item : NULL;
}

static enum listbox_match
match_option(struct listbox_item *item, struct terminal *term,
	     unsigned char *text)
{
	struct option *option = item->udata;

	if (option->type == OPT_TREE)
		return LISTBOX_MATCH_IMPOSSIBLE;

	if (strcasestr(option->name, text)
	    || (option->capt && strcasestr(_(option->capt, term), text)))
		return LISTBOX_MATCH_OK;

	return LISTBOX_MATCH_NO;
}

static int
can_delete_option(struct listbox_item *item)
{
	struct option *option = item->udata;

	if (option->root) {
		struct option *parent_option = option->root;

		return parent_option->flags & OPT_AUTOCREATE;
	}

	return 0;
}

static void
delete_option_item(struct listbox_item *item, int last)
{
	struct option *option = item->udata;

	assert(!is_object_used(option));

	/* Only built-in options needs to be marked as deleted, so if the
	 * option is allocated call the cleaner. */
	if (option->flags & OPT_ALLOC)
		delete_option(option);
	else
		mark_option_as_deleted(option);
}

static const struct listbox_ops options_listbox_ops = {
	lock_option,
	unlock_option,
	is_option_used,
	get_option_text,
	get_option_info,
	NULL,
	get_option_root,
	match_option,
	can_delete_option,
	delete_option_item,
	NULL,
	NULL,
};

/* Button handlers */

static widget_handler_status_T
check_valid_option(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct terminal *term = dlg_data->win->term;
	struct option *option = dlg_data->dlg->udata;
	struct session *ses = dlg_data->dlg->udata2;
	unsigned char *value = widget_data->cdata;
	unsigned char *chinon;
	int dummy_line = 0;

	commandline = 1;
	chinon = option_types[option->type].read(option, &value, &dummy_line);
	if (chinon) {
		if (option_types[option->type].set &&
		    option_types[option->type].set(option, chinon)) {
			option_changed(ses, option);

			commandline = 0;
			mem_free(chinon);
			return EVENT_PROCESSED;
		}
		mem_free(chinon);
	}
	commandline = 0;

	info_box(term, 0,
		 N_("Error"), ALIGN_LEFT,
		 N_("Bad option value."));

	return EVENT_NOT_PROCESSED;
}

static void
build_edit_dialog(struct terminal *term, struct session *ses,
		  struct option *option)
{
	/* [gettext_accelerator_context(.build_edit_dialog)] */
#define EDIT_WIDGETS_COUNT 5
	struct dialog *dlg;
	unsigned char *value, *name, *desc, *range;
	struct string tvalue;

	if (!init_string(&tvalue)) return;

	commandline = 1;
	option_types[option->type].write(option, &tvalue);
	commandline = 0;

	/* Create the dialog */
	dlg = calloc_dialog(EDIT_WIDGETS_COUNT, MAX_STR_LEN);
	if (!dlg) {
		done_string(&tvalue);
		return;
	}

	dlg->title = _("Edit", term);
	dlg->layouter = generic_dialog_layouter;
	dlg->udata = option;
	dlg->udata2 = ses;

	value = get_dialog_offset(dlg, EDIT_WIDGETS_COUNT);
	safe_strncpy(value, tvalue.source, MAX_STR_LEN);
	done_string(&tvalue);

	name = straconcat(_("Name", term), ": ", option->name, "\n",
			  _("Type", term), ": ",
			  _(option_types[option->type].name, term),
			  (unsigned char *) NULL);
	desc = straconcat(_("Description", term), ": \n",
			  _(option->desc ? option->desc
				  	 : (unsigned char *) "N/A", term),
			  (unsigned char *) NULL);
	range = get_range_string(option);
	if (range) {
		if (*range) {
			unsigned char *tmp;

			tmp = straconcat(name, " ", range,
					 (unsigned char *) NULL);
			if (tmp) {
				mem_free(name);
				name = tmp;
			}
		}
		mem_free(range);
	}

	if (!name || !desc) {
		mem_free_if(name);
		mem_free_if(desc);
		mem_free(dlg);
		return;
	}

	/* FIXME: Compute some meaningful maximal width. --pasky */
	add_dlg_text(dlg, name, ALIGN_LEFT, 0);
	add_dlg_field_float(dlg, _("Value", term), 0, 0, check_valid_option, MAX_STR_LEN, value, NULL);

	add_dlg_text(dlg, desc, ALIGN_LEFT, 0);

	add_dlg_button(dlg, _("~OK", term), B_ENTER, ok_dialog, NULL);
	add_dlg_button(dlg, _("~Cancel", term), B_ESC, cancel_dialog, NULL);

	add_dlg_end(dlg, EDIT_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, (void *) name, (void *) desc, (void *) NULL));
#undef EDIT_WIDGETS_COUNT
}

static widget_handler_status_T
push_edit_button(struct dialog_data *dlg_data,
		 struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct option *option;

	/* Show history item info */
	if (!box->sel || !box->sel->udata) return EVENT_PROCESSED;
	option = box->sel->udata;

	if (!option_types[option->type].write ||
	    !option_types[option->type].read ||
	    !option_types[option->type].set) {
		info_box(term, 0,
			 N_("Edit"), ALIGN_LEFT,
			 N_("This option cannot be edited. This means that "
			    "this is some special option like a folder - try "
			    "to press a space in order to see its contents."));
		return EVENT_PROCESSED;
	}

	build_edit_dialog(term, dlg_data->dlg->udata, option);

	return EVENT_PROCESSED;
}


struct add_option_to_tree_ctx {
	struct option *option;
	struct widget_data *widget_data;
};

static void
add_option_to_tree(void *data, unsigned char *name)
{
	struct add_option_to_tree_ctx *ctx = data;
	struct option *old = get_opt_rec_real(ctx->option, name);
	struct option *new;

	if (old && (old->flags & OPT_DELETED)) delete_option(old);
	/* get_opt_rec() will create the option. */
	new = get_opt_rec(ctx->option, name);
	if (new) listbox_sel(ctx->widget_data, new->box_item);
	/* TODO: If the return value is NULL, we should pop up a msgbox. */
}

static widget_handler_status_T
check_option_name(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	unsigned char *p;

	for (p = widget_data->cdata; *p; p++)
		if (!is_option_name_char(*p)) {
			info_box(dlg_data->win->term, 0,
				 N_("Bad string"), ALIGN_CENTER,
				 N_("Option names may only contain alpha-numeric characters\n"
				 "in addition to '_', '-', '+', and '*'."));
			return EVENT_NOT_PROCESSED;
		}

	return EVENT_PROCESSED;
}

static widget_handler_status_T
push_add_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct listbox_item *item = box->sel;
	struct option *option;
	struct add_option_to_tree_ctx *ctx;

	if (!item || !item->udata) {

invalid_option:
		info_box(term, 0, N_("Add option"), ALIGN_CENTER,
			 N_("Cannot add an option here."));
		return EVENT_PROCESSED;
	}


	if (item->type == BI_FOLDER && !item->expanded) {
		item = box->ops->get_root(item);
		if (!item || !item->udata)
			goto invalid_option;
	}

	option = item->udata;

	if (!(option->flags & OPT_AUTOCREATE)) {
		if (option->root) option = option->root;
		if (!option || !(option->flags & OPT_AUTOCREATE))
			goto invalid_option;
	}

	ctx = mem_alloc(sizeof(*ctx));
	if (!ctx) return EVENT_PROCESSED;
	ctx->option = option;
	ctx->widget_data = dlg_data->widgets_data;

	input_dialog(term, getml(ctx, (void *) NULL), N_("Add option"), N_("Name"),
		     ctx, NULL,
		     MAX_STR_LEN, "", 0, 0, check_option_name,
		     add_option_to_tree, NULL);

	return EVENT_PROCESSED;
}


static widget_handler_status_T
push_save_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	write_config(dlg_data->win->term);

	update_hierbox_browser(&option_browser);

	return EVENT_PROCESSED;
}


static const struct hierbox_browser_button option_buttons[] = {
	/* [gettext_accelerator_context(.option_buttons)] */
	{ N_("~Info"),   push_hierbox_info_button,   1 },
	{ N_("~Edit"),   push_edit_button,           0 },
	{ N_("~Add"),    push_add_button,            0 },
	{ N_("~Delete"), push_hierbox_delete_button, 0 },
	{ N_("~Search"), push_hierbox_search_button, 1 },
	{ N_("Sa~ve"),   push_save_button,           0 },
};

struct_hierbox_browser(
	option_browser,
	N_("Option manager"),
	option_buttons,
	&options_listbox_ops
);

/* Builds the "Options manager" dialog */
void
options_manager(struct session *ses)
{
	hierbox_browser(&option_browser, ses);
}


/****************************************************************************
  Keybinding manager stuff.
****************************************************************************/

#ifdef CONFIG_SMALL
static int keybinding_text_toggle = 1;
#else
static int keybinding_text_toggle;
#endif

/* XXX: ACTION_BOX_SIZE is just a quick hack, we ought to allocate
 * the sub-arrays separately. --pasky */
#define ACTION_BOX_SIZE 128
static struct listbox_item *action_box_items[KEYMAP_MAX][ACTION_BOX_SIZE];

struct listbox_item *
get_keybinding_action_box_item(enum keymap_id keymap_id, action_id_T action_id)
{
	assert(action_id < ACTION_BOX_SIZE);
	if_assert_failed return NULL;

	return action_box_items[keymap_id][action_id];
}

struct listbox_item *keymap_box_item[KEYMAP_MAX];

void
init_keybinding_listboxes(struct keymap keymap_table[KEYMAP_MAX],
			  const struct action_list actions[])
{
	struct listbox_item *root = &keybinding_browser.root;
	const struct action *act;
	enum keymap_id keymap_id;

	/* Do it backwards because add_listbox_item() add to front
	 * of list. */
	for (keymap_id = 0; keymap_id < KEYMAP_MAX; keymap_id++) {
		struct listbox_item *keymap_box;

		keymap_box = add_listbox_item(NULL, root, BI_FOLDER, &keymap_table[keymap_id], -1);
		if (!keymap_box) continue;

		for (act = actions[keymap_id].actions; act->str; act++) {
			struct listbox_item *item;

			assert(act->num < ACTION_BOX_SIZE);
			if_assert_failed continue;

			if (act->num == ACT_MAIN_SCRIPTING_FUNCTION
			    || act->num == ACT_MAIN_NONE)
				continue;

#ifndef CONFIG_SMALL
			assert(act->desc);
#endif

			item = add_listbox_item(NULL, keymap_box, BI_FOLDER,
						(void *) act, -1);
			if (!item) continue;

			item->expanded = 1;

			action_box_items[keymap_id][act->num] = item;
		}

		keymap_box_item[keymap_id] = keymap_box;
	}
}

void
done_keybinding_listboxes(void)
{
	struct listbox_item *action;

	foreach (action, keybinding_browser.root.child) {
		struct listbox_item *keymap;

		foreach (keymap, action->child) {
			free_list(keymap->child);
		}
		free_list(action->child);
	}
	free_list(keybinding_browser.root.child);
}


/* Implementation of the listbox operations */

/* XXX: If anything but delete button will use these object_*() requiring
 * functions we have to check if it is action or keymap box items. */

static void
lock_keybinding(struct listbox_item *item)
{
	if (item->depth == 2)
		object_lock((struct keybinding *) item->udata);
}

static void
unlock_keybinding(struct listbox_item *item)
{
	if (item->depth == 2)
		object_unlock((struct keybinding *) item->udata);
}

static int
is_keybinding_used(struct listbox_item *item)
{
	if (item->depth != 2) return 0;
	return is_object_used((struct keybinding *) item->udata);
}

static unsigned char *
get_keybinding_text(struct listbox_item *item, struct terminal *term)
{
	struct keybinding *keybinding = item->udata;
	struct string info;

	if (item->depth == 0) {
		struct keymap *keymap = item->udata;

		return stracpy(keybinding_text_toggle ? keymap->str
		                                      : _(keymap->desc, term));
	} else if (item->depth < 2) {
		const struct action *action = item->udata;

		return stracpy(keybinding_text_toggle ? action->str
		                                      : _(action->desc, term));
	}

	if (!init_string(&info)) return NULL;
	add_keystroke_to_string(&info, &keybinding->kbd, 0);
	return info.source;
}

static unsigned char *
get_keybinding_info(struct listbox_item *item, struct terminal *term)
{
	struct keybinding *keybinding = item->udata;
	unsigned char *action, *keymap;
	struct string info;

	if (item->depth < 2) return NULL;
	if (item->type == BI_FOLDER) return NULL;

	if (!init_string(&info))
		return NULL;

	action = get_action_name(keybinding->keymap_id, keybinding->action_id);
	keymap = get_keymap_name(keybinding->keymap_id);

	add_format_to_string(&info, "%s: ", _("Keystroke", term));
	add_keystroke_to_string(&info, &keybinding->kbd, 0);
	add_format_to_string(&info, "\n%s: %s", _("Action", term), action);
	add_format_to_string(&info, "\n%s: %s", _("Keymap", term), keymap);

	return info.source;
}

static struct listbox_item *
get_keybinding_root(struct listbox_item *item)
{
	/* .. at the bottom */
	if (item->depth == 0) return NULL;

	if (item->depth == 1) {
		const struct action *action = item->udata;

		return keymap_box_item[action->keymap_id];
	} else {
		struct keybinding *kb = item->udata;

		return get_keybinding_action_box_item(kb->keymap_id, kb->action_id);
	}
}

static enum listbox_match
match_keybinding(struct listbox_item *item, struct terminal *term,
		 unsigned char *text)
{
	const struct action *action = item->udata;
	unsigned char *desc;

	if (item->depth != 1)
		return LISTBOX_MATCH_IMPOSSIBLE;

	desc = keybinding_text_toggle
	     ? action->str : _(action->desc, term);

	if ((desc && strcasestr(desc, text)))
		return LISTBOX_MATCH_OK;

	return LISTBOX_MATCH_NO;
}

static int
can_delete_keybinding(struct listbox_item *item)
{
	return item->depth == 2;
}


static void
delete_keybinding_item(struct listbox_item *item, int last)
{
	struct keybinding *keybinding = item->udata;

	assert(item->depth == 2 && !is_object_used(keybinding));

	free_keybinding(keybinding);
}

static const struct listbox_ops keybinding_listbox_ops = {
	lock_keybinding,
	unlock_keybinding,
	is_keybinding_used,
	get_keybinding_text,
	get_keybinding_info,
	NULL,
	get_keybinding_root,
	match_keybinding,
	can_delete_keybinding,
	delete_keybinding_item,
	NULL,
	NULL,
};


struct kbdbind_add_hop {
	struct terminal *term;
	action_id_T action_id;
	enum keymap_id keymap_id;
	struct term_event_keyboard kbd;
	struct widget_data *widget_data;
};

static struct kbdbind_add_hop *
new_hop_from(struct kbdbind_add_hop *hop)
{
	struct kbdbind_add_hop *new_hop = mem_alloc(sizeof(*new_hop));

	if (new_hop)
		copy_struct(new_hop, hop);

	return new_hop;
}

static void
really_really_add_keybinding(void *data)
{
	struct kbdbind_add_hop *hop = data;
	struct keybinding *keybinding;

	assert(hop);

	keybinding = add_keybinding(hop->keymap_id, hop->action_id, &hop->kbd,
	                            EVENT_NONE);

	if (keybinding && keybinding->box_item)
		listbox_sel(hop->widget_data, keybinding->box_item);
}

static void
really_add_keybinding(void *data, unsigned char *keystroke)
{
	/* [gettext_accelerator_context(.really_add_keybinding.yn)] */
	struct kbdbind_add_hop *hop = data;
	action_id_T action_id;

	/* check_keystroke() has parsed @keystroke to @hop->kbd.  */
	if (keybinding_exists(hop->keymap_id, &hop->kbd, &action_id)
	    && action_id != ACT_MAIN_NONE) {
		struct kbdbind_add_hop *new_hop;
		struct string canonical;

		/* Same keystroke for same action, just return. */
		if (action_id == hop->action_id) return;

		/* @*hop is on the memory_list of the input_dialog,
		 * which will be closed when this function returns.  */
		new_hop = new_hop_from(hop);
		if (!new_hop) return; /* out of mem */

		/* Try to convert the parsed keystroke back to a
		 * string, so that the "Keystroke already used" box
		 * displays the same canonical name as the keybinding
		 * manager does.  If something goes wrong here, then
		 * canonical.length will probably be 0, in which case
		 * we'll use the original @keystroke string instead. */
		if (init_string(&canonical))
			add_keystroke_to_string(&canonical, &hop->kbd, 0);

		msg_box(new_hop->term, getml(new_hop, (void *) NULL), MSGBOX_FREE_TEXT,
			N_("Keystroke already used"), ALIGN_CENTER,
			msg_text(new_hop->term, N_("The keystroke \"%s\" "
				 "is currently used for \"%s\".\n"
				 "Are you sure you want to replace it?"),
				 canonical.length ? canonical.source : keystroke,
				 get_action_name(hop->keymap_id, action_id)),
			new_hop, 2,
			MSG_BOX_BUTTON(N_("~Yes"), really_really_add_keybinding, B_ENTER),
			MSG_BOX_BUTTON(N_("~No"), NULL, B_ESC));

		done_string(&canonical); /* safe even if init failed */
		return;
	}

	really_really_add_keybinding((void *) hop);
}

static widget_handler_status_T
check_keystroke(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct kbdbind_add_hop *hop = dlg_data->dlg->udata2;
	unsigned char *keystroke = widget_data->cdata;

	if (parse_keystroke(keystroke, &hop->kbd) >= 0)
		return EVENT_PROCESSED;

	info_box(hop->term, 0, N_("Add keybinding"), ALIGN_CENTER,
		 N_("Invalid keystroke."));

	return EVENT_NOT_PROCESSED;
}

static widget_handler_status_T
push_kbdbind_add_button(struct dialog_data *dlg_data,
			struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct listbox_item *item = box->sel;
	struct kbdbind_add_hop *hop;
	unsigned char *text;

	if (!item || !item->depth) {
		info_box(term, 0, N_("Add keybinding"), ALIGN_CENTER,
			 N_("Need to select an action."));
		return EVENT_PROCESSED;
	}

	hop = mem_calloc(1, sizeof(*hop));
	if (!hop) return EVENT_PROCESSED;
	hop->term = term;
	hop->widget_data = dlg_data->widgets_data;

	if (item->depth == 2) {
		struct keybinding *keybinding = item->udata;

		hop->action_id = keybinding->action_id;
		hop->keymap_id = keybinding->keymap_id;
	} else {
		const struct action *action = item->udata;

		hop->action_id = action->num;
		hop->keymap_id = action->keymap_id;
	}

	text = msg_text(term,
			N_("Action: %s\n"
			   "Keymap: %s\n"
			   "\n"
			   "Keystroke should be written in the format: "
			   "[Shift-][Ctrl-][Alt-]Key\n"
			   "Key: a,b,c,...,1,2,3,...,Space,Up,PageDown,"
			   "Tab,Enter,Insert,F5,..."
			   "\n\n"
			   "Keystroke"),
			get_action_name(hop->keymap_id, hop->action_id),
			get_keymap_name(hop->keymap_id));

	input_dialog(term, getml(hop, (void *) text, (void *) NULL),
		     N_("Add keybinding"), text,
		     hop, NULL,
		     MAX_STR_LEN, "", 0, 0, check_keystroke,
		     really_add_keybinding, NULL);

	return EVENT_PROCESSED;
}


static widget_handler_status_T
push_kbdbind_toggle_display_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
#ifndef CONFIG_SMALL
	keybinding_text_toggle = !keybinding_text_toggle;
	redraw_dialog(dlg_data, 0);
#endif
	return EVENT_PROCESSED;
}


/* FIXME: Races here, we need to lock the entry..? --pasky */

static widget_handler_status_T
push_kbdbind_save_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	write_config(dlg_data->win->term);
	return EVENT_PROCESSED;
}


static const struct hierbox_browser_button keybinding_buttons[] = {
	/* [gettext_accelerator_context(.keybinding_buttons)] */
	{ N_("~Add"),            push_kbdbind_add_button,            0 },
	{ N_("~Delete"),         push_hierbox_delete_button,         0 },
	{ N_("~Toggle display"), push_kbdbind_toggle_display_button, 1 },
	{ N_("~Search"),         push_hierbox_search_button,         1 },
	{ N_("Sa~ve"),           push_kbdbind_save_button,           0 },
};

struct_hierbox_browser(
	keybinding_browser,
	N_("Keybinding manager"),
	keybinding_buttons,
	&keybinding_listbox_ops
);

/* Builds the "Keybinding manager" dialog */
void
keybinding_manager(struct session *ses)
{
	hierbox_browser(&keybinding_browser, ses);
}
