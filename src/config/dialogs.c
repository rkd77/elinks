/* Options dialogs */
/* $Id: dialogs.c,v 1.201.4.8 2005/04/06 09:11:18 jonas Exp $ */

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
#include "sched/session.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/secsave.h"


static void
toggle_success_msgbox(void *dummy)
{
	get_opt_bool("ui.success_msgbox") = !get_opt_bool("ui.success_msgbox");
	get_opt_rec(config_options, "ui.success_msgbox")->flags |= OPT_TOUCHED;
}

void
write_config_dialog(struct terminal *term, unsigned char *config_file,
		    int secsave_error, int stdio_error)
{
	unsigned char *errmsg = NULL;
	unsigned char *strerr;

	if (secsave_error == SS_ERR_NONE && !stdio_error) {
		if (!get_opt_bool("ui.success_msgbox")) return;

		msg_box(term, NULL, MSGBOX_FREE_TEXT,
			N_("Write config success"), ALIGN_CENTER,
			msg_text(term, N_("Options were saved successfully to config file %s."),
				 config_file),
			NULL, 2,
			N_("~OK"), NULL, B_ENTER | B_ESC,
			N_("~Do not show anymore"), toggle_success_msgbox, 0);
		return;
	}

	switch (secsave_error) {
		case SS_ERR_OPEN_READ:
			strerr = _("Cannot read the file", term);
			break;
		case SS_ERR_STAT:
			strerr = _("Cannot get file status", term);
			break;
		case SS_ERR_ACCESS:
			strerr = _("Cannot access the file", term);
			break;
		case SS_ERR_MKSTEMP:
			strerr = _("Cannot create temp file", term);
			break;
		case SS_ERR_RENAME:
			strerr = _("Cannot rename the file", term);
			break;
		case SS_ERR_DISABLED:
			strerr = _("File saving disabled by option", term);
			break;
		case SS_ERR_OUT_OF_MEM:
			strerr = _("Out of memory", term);
			break;
		case SS_ERR_OPEN_WRITE:
			strerr = _("Cannot write the file", term);
			break;
		case SS_ERR_NONE: /* Impossible. */
		case SS_ERR_OTHER:
		default:
			strerr = _("Secure file saving error", term);
			break;
	}

	if (stdio_error > 0)
		errmsg = straconcat(strerr, " (", strerror(stdio_error), ")", NULL);

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
				  " (", _("modified", term), ")", NULL);

	return stracpy(_(desc, term));
}

static unsigned char *
get_option_info(struct listbox_item *item, struct terminal *term)
{
	struct option *option = item->udata;
	unsigned char *desc, *type;
	struct string info;

	if (!init_string(&info)) return NULL;

	type = _(option_types[option->type].name, term);
	if (option->type == OPT_TREE) {
		type = straconcat(type, " ",
				_("(expand by pressing space)", term), NULL);
	}

	desc = _(option->desc  ? option->desc : (unsigned char *) "N/A", term);

	if (option_types[option->type].write) {
		unsigned char *range;
		struct string value;

		if (!init_string(&value)) {
			done_string(&info);
			return NULL;
		}

		option_types[option->type].write(option, &value);

		add_format_to_string(&info, "%s: %s", _("Name", term), option->name);
		add_format_to_string(&info, "\n%s: %s", _("Type", term), type);
		range = get_range_string(option);
		if (range) {
			if (*range) {
				add_to_string(&info, " ");
				add_to_string(&info, range);
			}
			mem_free(range);
		}
		add_format_to_string(&info, "\n%s: %s", _("Value", term), value.source);

		if (option->flags & OPT_TOUCHED)
			add_to_string(&info, _("\n\nThis value has been changed"
					     " since you last saved your"
					     " configuration.", term));

		if (*desc)
			add_format_to_string(&info, "\n\n%s:\n%s", _("Description", term), desc);
		done_string(&value);
	} else {
		add_format_to_string(&info, "%s: %s", _("Name", term), option->name);
		add_format_to_string(&info, "\n%s: %s", _("Type", term), type);
		if (*desc)
			add_format_to_string(&info, "\n\n%s:\n%s", _("Description", term), desc);
	}

	if (option->type == OPT_TREE) {
		mem_free(type);
	}

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

static struct listbox_ops options_listbox_ops = {
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

static t_handler_event_status
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
			struct option *current = option;

			option->flags |= OPT_TOUCHED;

			/* Notify everyone out there! */

			/* This boolean thing can look a little weird - it
			 * basically says that we should proceed when there's
			 * no change_hook or there's one and its return value
			 * was zero. */
			while (current && (!current->change_hook ||
				!current->change_hook(ses, current, option))) {
				if (!current->root)
					break;

				current = current->root;
			}

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
			  _(option_types[option->type].name, term), NULL);
	desc = straconcat(_("Description", term), ": \n",
			  _(option->desc ? option->desc
				  	 : (unsigned char *) "N/A", term),
			  NULL);
	range = get_range_string(option);
	if (range) {
		if (*range) {
			unsigned char *tmp;

			tmp = straconcat(name, " ", range, NULL);
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

	do_dialog(term, dlg, getml(dlg, name, desc, NULL));
#undef EDIT_WIDGETS_COUNT
}

static t_handler_event_status
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


static void
add_option_to_tree(void *data, unsigned char *name)
{
	struct option *option = data;
	struct option *old = get_opt_rec_real(option, name);

	if (old && (old->flags & OPT_DELETED)) delete_option(old);
	/* get_opt_rec() will do all the work for ourselves... ;-) */
	get_opt_rec(option, name);
	/* TODO: If the return value is NULL, we should pop up a msgbox. */
}

static t_handler_event_status
push_add_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	struct terminal *term = dlg_data->win->term;
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct listbox_item *item = box->sel;
	struct option *option;

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

	input_dialog(term, NULL, N_("Add option"), N_("Name"),
		     option, NULL,
		     MAX_STR_LEN, "", 0, 0, check_nonempty,
		     add_option_to_tree, NULL);
	
	return EVENT_PROCESSED;
}


static t_handler_event_status
push_save_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	write_config(dlg_data->win->term);

	update_hierbox_browser(&option_browser);

	return EVENT_PROCESSED;
}

#define	OPTION_MANAGER_BUTTONS	5

static struct hierbox_browser_button option_buttons[] = {
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
get_keybinding_action_box_item(enum keymap km, int action)
{
	assert(action < ACTION_BOX_SIZE);
	if_assert_failed return NULL;

	return action_box_items[km][action];
}

struct keymap_box_item_info {
	struct listbox_item *box_item;
	struct strtonum *first, *last;
};

struct keymap_box_item_info keymap_box_item_info[KEYMAP_MAX];

void
init_keybinding_listboxes(struct strtonum *keymaps, struct strtonum *actions[])
{
	struct listbox_item *root = &keybinding_browser.root;
	struct strtonum *act, *map;

	/* Do it backwards because add_listbox_item() add to front
	 * of list. */
	for (map = keymaps; map->str; map++) {
		struct listbox_item *keymap;

		keymap = add_listbox_item(NULL, root, BI_FOLDER, map, -1);
		if (!keymap) continue;

		keymap_box_item_info[map->num].box_item = keymap;

		for (act = actions[map->num]; act->str; act++) {
			struct listbox_item *item;

			assert(act->num < ACTION_BOX_SIZE);
			if_assert_failed continue;

			if (act->num == ACT_MAIN_SCRIPTING_FUNCTION
			    || act->num == ACT_MAIN_NONE)
				continue;

#ifndef CONFIG_SMALL
			assert(act->desc);
#endif

			item = add_listbox_item(NULL, keymap, BI_FOLDER, act, -1);
			if (!item) continue;

			item->expanded = 1;

			action_box_items[map->num][act->num] = item;
		}

		keymap_box_item_info[map->num].first = actions[map->num];
		keymap_box_item_info[map->num].last = act;
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
	if (item->depth == 2) return 0;
	return is_object_used((struct keybinding *) item->udata);
}

static unsigned char *
get_keybinding_text(struct listbox_item *item, struct terminal *term)
{
	struct keybinding *keybinding = item->udata;
	unsigned char *keymap;
	struct string info;

	if (item->depth < 2) {
		struct strtonum *strtonum = item->udata;

		keymap = keybinding_text_toggle
			? strtonum->str : _(strtonum->desc, term);
		return stracpy(keymap);
	}

	if (!init_string(&info)) return NULL;
	make_keystroke(&info, keybinding->key, keybinding->meta, 0);
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

	action = write_action(keybinding->keymap, keybinding->action);
	keymap = write_keymap(keybinding->keymap);

	add_format_to_string(&info, "%s: ", _("Keystroke", term));
	make_keystroke(&info, keybinding->key, keybinding->meta, 0);
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
		struct strtonum *action = item->udata;
		int keymap;

		for (keymap = 0; keymap < KEYMAP_MAX; keymap++) {
			if (keymap_box_item_info[keymap].first <= action
			    && keymap_box_item_info[keymap].last > action)
				return keymap_box_item_info[keymap].box_item;
		}

		return NULL;
	} else {
		struct keybinding *kb = item->udata;

		return get_keybinding_action_box_item(kb->keymap, kb->action);
	}
}

static enum listbox_match
match_keybinding(struct listbox_item *item, struct terminal *term,
		 unsigned char *text)
{
	struct strtonum *strtonum = item->udata;
	unsigned char *desc;

	if (item->depth != 1)
		return LISTBOX_MATCH_IMPOSSIBLE;

	desc = keybinding_text_toggle
	     ? strtonum->str : _(strtonum->desc, term);

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

static struct listbox_ops keybinding_listbox_ops = {
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
	int action, keymap;
	long key, meta;
};

struct kbdbind_add_hop *
new_hop_from(struct kbdbind_add_hop *hop) {
	struct kbdbind_add_hop *new_hop = mem_alloc(sizeof(*new_hop));

	if (new_hop)
		copy_struct(new_hop, hop);

	return new_hop;
}

static void
really_really_add_keybinding(void *data)
{
	struct kbdbind_add_hop *hop = data;

	assert(hop);

	add_keybinding(hop->keymap, hop->action, hop->key, hop->meta, 0);
}

static void
really_add_keybinding(void *data, unsigned char *keystroke)
{
	struct kbdbind_add_hop *hop = data;
	int action;

	if (keybinding_exists(hop->keymap, hop->key, hop->meta, &action)
	    && action != ACT_MAIN_NONE) {
		struct kbdbind_add_hop *new_hop;

		/* Same keystroke for same action, just return. */
		if (action == hop->action) return;

		new_hop = new_hop_from(hop);
		if (!new_hop) return; /* out of mem */

		msg_box(new_hop->term, getml(new_hop, NULL), MSGBOX_FREE_TEXT,
			N_("Keystroke already used"), ALIGN_CENTER,
			msg_text(new_hop->term, N_("The keystroke \"%s\" "
			"is currently used for \"%s\".\n"
			"Are you sure you want to replace it?"),
			keystroke, write_action(hop->keymap, action)),
			new_hop, 2,
			N_("~Yes"), really_really_add_keybinding, B_ENTER,
			N_("~No"), NULL, B_ESC);

		return;
	}

	really_really_add_keybinding((void *) hop);
}

t_handler_event_status
check_keystroke(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct kbdbind_add_hop *hop = dlg_data->dlg->udata2;
	unsigned char *keystroke = widget_data->cdata;

	if (parse_keystroke(keystroke, &hop->key, &hop->meta) >= 0)
		return EVENT_PROCESSED;

	info_box(hop->term, 0, N_("Add keybinding"), ALIGN_CENTER,
		 N_("Invalid keystroke."));

	return EVENT_NOT_PROCESSED;
}

static t_handler_event_status
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
			 N_("Need to select a keymap."));
		return EVENT_PROCESSED;
	}

	hop = mem_calloc(1, sizeof(*hop));
	if (!hop) return EVENT_PROCESSED;
	hop->term = term;

	if (item->depth == 2) {
		struct keybinding *keybinding = item->udata;

		hop->action = keybinding->action;
		hop->keymap = keybinding->keymap;
	} else {
		struct strtonum *strtonum = item->udata;

		hop->action = strtonum->num;

		item = get_keybinding_root(item);
		if (!item) {
			mem_free(hop);
			return EVENT_PROCESSED;
		}

		strtonum = item->udata;
		hop->keymap = strtonum->num;
	}

	text = msg_text(term,
			"Action: %s\n"
			"Keymap: %s\n"
			"\n"
			"Keystroke should be written in the format: "
			"[Prefix-]Key\n"
			"Prefix: Shift, Ctrl, Alt\n"
			"Key: a,b,c,...,1,2,3,...,Space,Up,PageDown,"
			"Tab,Enter,Insert,F5,..."
			"\n\n"
			"Keystroke",
			write_action(hop->keymap, hop->action),
			write_keymap(hop->keymap));

	input_dialog(term, getml(hop, text, NULL),
		     N_("Add keybinding"), text,
		     hop, NULL,
		     MAX_STR_LEN, "", 0, 0, check_keystroke,
		     really_add_keybinding, NULL);

	return EVENT_PROCESSED;
}


static t_handler_event_status
push_kbdbind_toggle_display_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
#ifndef CONFIG_SMALL
	keybinding_text_toggle = !keybinding_text_toggle;
	clear_dialog(dlg_data, some_useless_info_button);
#endif
	return EVENT_PROCESSED;
}


/* FIXME: Races here, we need to lock the entry..? --pasky */

static t_handler_event_status
push_kbdbind_save_button(struct dialog_data *dlg_data,
		struct widget_data *some_useless_info_button)
{
	write_config(dlg_data->win->term);
	return EVENT_PROCESSED;
}

#define	KEYBINDING_MANAGER_BUTTONS	4

static INIT_LIST_HEAD(keybinding_dialog_list);

static struct hierbox_browser_button keybinding_buttons[] = {
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
