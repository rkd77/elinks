/* Implementation of a login manager for HTML forms */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/home.h"
#include "document/forms.h"
#include "formhist/dialogs.h"
#include "formhist/formhist.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "main/object.h"
#include "session/session.h"
#include "terminal/window.h"
#include "util/base64.h"
#include "util/file.h"
#include "util/lists.h"
#include "util/secsave.h"
#include "util/string.h"
#include "viewer/text/form.h"

#define FORMS_HISTORY_FILENAME		"formhist"


/* TODO: Remember multiple login for the same form.
 * TODO: Password manager GUI (here?) (in dialogs.c, of course --pasky). */


static union option_info forms_history_options[] = {
	INIT_OPT_BOOL("document.browse.forms", N_("Show form history dialog"),
		"show_formhist", 0, 0,
		N_("Ask if a login form should be saved to file or not. "
		"This option only disables the dialog, already saved login "
		"forms are unaffected.")),

	NULL_OPTION_INFO,
};

INIT_LIST_OF(struct formhist_data, saved_forms);

static struct formhist_data *
new_formhist_item(unsigned char *url)
{
	struct formhist_data *form;
	int url_len = strlen(url);

	form = mem_calloc(1, sizeof(*form) + url_len);
	if (!form) return NULL;

	memcpy(form->url, url, url_len);
	form->submit = mem_alloc(sizeof(*form->submit));
	if (!form->submit) { mem_free(form); return NULL; }

	object_nolock(form, "formhist");
	init_list(*form->submit);
	form->box_item = add_listbox_leaf(&formhist_browser, NULL, form);
	if (!form->box_item) {
		mem_free(form->submit);
		mem_free(form);
		return NULL;
	}

	return form;
}

static void
done_formhist_item(struct formhist_data *form)
{
	done_listbox_item(&formhist_browser, form->box_item);
	done_submitted_value_list(form->submit);
	mem_free(form->submit);
	mem_free(form);
}

void
delete_formhist_item(struct formhist_data *form)
{
	del_from_list(form);
	done_formhist_item(form);
}

static int loaded = 0;

int
load_formhist_from_file(void)
{
	struct formhist_data *form;
	unsigned char tmp[MAX_STR_LEN];
	unsigned char *file;
	FILE *f;

	if (loaded) return 1;

	if (!elinks_home) return 0;

	file = straconcat(elinks_home, FORMS_HISTORY_FILENAME,
			  (unsigned char *) NULL);
	if (!file) return 0;

	f = fopen(file, "rb");
	mem_free(file);
	if (!f) return 0;

	while (fgets(tmp, MAX_STR_LEN, f)) {
		unsigned char *p;
		int dontsave = 0;

		if (tmp[0] == '\n' && !tmp[1]) continue;

		p = strchr(tmp, '\t');
		if (p) {
			*p = '\0';
			++p;
			if (!strcmp(tmp, "dontsave"))
				dontsave = 1;
		} else {
			/* Compat. with older file formats. Remove it at some
			 * time. --Zas */
			if (!strncmp(tmp, "dontsave,", 9)) {
				dontsave = 1;
				p = tmp + 9;
			} else {
				p = tmp;
			}
		}

		/* URL */
		p[strlen(p) - 1] = '\0';

		form = new_formhist_item(p);
		if (!form) continue;
		if (dontsave) form->dontsave = 1;

		/* Fields type, name, value */
		while (fgets(tmp, MAX_STR_LEN, f)) {
			struct submitted_value *sv;
			unsigned char *type, *name, *value;
			unsigned char *enc_value;
			enum form_type ftype;
			int ret;

			if (tmp[0] == '\n' && !tmp[1]) break;

			/* Type */
			type = tmp;
			p = strchr(type, '\t');
			if (!p) goto fail;
			*p = '\0';

			/* Name */
			name = ++p;
			p = strchr(name, '\t');
			if (!p) {
				/* Compatibility with previous file formats.
				 * REMOVE AT SOME TIME --Zas */
				value = name;
				name = type;

				if (*name == '*') {
					name++;
					type = "password";
				} else {
					type = "text";
				}

				goto cont;
			}
			*p = '\0';

			/* Value */
			value = ++p;
cont:
			p = strchr(value, '\n');
			if (!p) goto fail;
			*p = '\0';

			ret = str2form_type(type);
			if (ret == -1) goto fail;
			ftype = ret;

			if (form->dontsave) continue;

			enc_value = *value ? base64_decode(value)
					   : stracpy(value);
			if (!enc_value) goto fail;

			sv = init_submitted_value(name, enc_value,
						 ftype, NULL, 0);

			mem_free(enc_value);
			if (!sv) goto fail;

			add_to_list(*form->submit, sv);
		}

		add_to_list(saved_forms, form);
	}

	fclose(f);
	loaded = 1;

	return 1;

fail:
	done_formhist_item(form);
	return 0;
}

int
save_formhist_to_file(void)
{
	struct secure_save_info *ssi;
	unsigned char *file;
	struct formhist_data *form;
	int r;

	if (!elinks_home || get_cmd_opt_bool("anonymous"))
		return 0;

	file = straconcat(elinks_home, FORMS_HISTORY_FILENAME,
			  (unsigned char *) NULL);
	if (!file) return 0;

	ssi = secure_open(file);
	mem_free(file);
	if (!ssi) return 0;

	/* Write the list to password file ($ELINKS_HOME/formhist) */

	foreach (form, saved_forms) {
		struct submitted_value *sv;

		if (form->dontsave) {
			secure_fprintf(ssi, "dontsave\t%s\n\n", form->url);
			continue;
		}

		secure_fprintf(ssi, "%s\n", form->url);

		foreach (sv, *form->submit) {
			unsigned char *encvalue;

			if (sv->value && *sv->value) {
				/* Obfuscate the value. If we do
				 * $ cat ~/.elinks/formhist
				 * we don't want someone behind our back to read our
				 * password (androids don't count). */
				encvalue = base64_encode(sv->value);
			} else {
				encvalue = stracpy("");
			}

			if (!encvalue) return 0;
			/* Format is : type[TAB]name[TAB]value[CR] */
			secure_fprintf(ssi, "%s\t%s\t%s\n", form_type2str(sv->type),
				       sv->name, encvalue);

			mem_free(encvalue);
		}

		secure_fputc(ssi, '\n');
	}

	r = secure_close(ssi);
	if (r == 0) loaded = 1;

	return r;
}

/* Check whether the form (chain of @submit submitted_values at @url document)
 * is already present in the form history. */
static int
form_exists(struct formhist_data *form1)
{
	struct formhist_data *form;

	if (!load_formhist_from_file()) return 0;

	foreach (form, saved_forms) {
		int count = 0;
		int exact = 0;
		struct submitted_value *sv;

		if (strcmp(form->url, form1->url)) continue;
		if (form->dontsave) return 1;

		/* Iterate through submitted entries. */
		foreach (sv, *form1->submit) {
			struct submitted_value *sv2;
			unsigned char *value = NULL;

			count++;
			foreach (sv2, *form->submit) {
				if (sv->type != sv2->type) continue;
				if (!strcmp(sv->name, sv2->name)) {
					exact++;
					value = sv2->value;
					break;
				}
			}
			/* If we found a value for that name, check if value
			 * has changed or not. */
			if (value && strcmp(sv->value, value)) return 0;
		}

		/* Check if submitted values have changed or not. */
		if (count && exact && count == exact) return 1;
	}

	return 0;
}

static int
forget_forms_with_url(unsigned char *url)
{
	struct formhist_data *form, *next;
	int count = 0;

	foreachsafe (form, next, saved_forms) {
		if (strcmp(form->url, url)) continue;

		delete_formhist_item(form);
		count++;
	}

	return count;
}

/* Appends form data @form_ (url and submitted_value(s)) to the password file. */
static void
remember_form(void *form_)
{
	struct formhist_data *form = form_;

	forget_forms_with_url(form->url);
	add_to_list(saved_forms, form);

	save_formhist_to_file();
}

static void
dont_remember_form(void *form_)
{
	struct formhist_data *form = form_;

	done_formhist_item(form);
}

static void
never_for_this_site(void *form_)
{
	struct formhist_data *form = form_;

	form->dontsave = 1;
	remember_form(form);
}

unsigned char *
get_form_history_value(unsigned char *url, unsigned char *name)
{
	struct formhist_data *form;

	if (!url || !*url || !name || !*name) return NULL;

	if (!load_formhist_from_file()) return NULL;

	foreach (form, saved_forms) {
		if (form->dontsave) continue;

		if (!strcmp(form->url, url)) {
			struct submitted_value *sv;

			foreach (sv, *form->submit)
				if (!strcmp(sv->name, name))
					return sv->value;
		}
	}

	return NULL;
}

void
memorize_form(struct session *ses, LIST_OF(struct submitted_value) *submit,
	      struct form *forminfo)
{
	/* [gettext_accelerator_context(memorize_form)] */
	struct formhist_data *form;
	struct submitted_value *sv;
	int save = 0;

	/* XXX: For now, we only save these types of form fields. */
	foreach (sv, *submit) {
		if (sv->type == FC_PASSWORD && sv->value && *sv->value) {
			save = 1;
			break;
		}
	}

	if (!save) return;

	/* Create a temporary form. */
	form = new_formhist_item(forminfo->action);
	if (!form) return;

	foreach (sv, *submit) {
		if ((sv->type == FC_TEXT) || (sv->type == FC_PASSWORD)) {
			struct submitted_value *sv2;

			sv2 = init_submitted_value(sv->name, sv->value,
					 	   sv->type, NULL, 0);
			if (!sv2) goto fail;

			add_to_list(*form->submit, sv2);
		}
	}

	if (form_exists(form)) goto fail;

	msg_box(ses->tab->term, NULL, 0,
		N_("Form history"), ALIGN_CENTER,
		N_("Should this login be remembered?\n\n"
		"Please note that the password will be stored "
		"obscured (but unencrypted) in a file on your disk.\n\n"
		"If you are using a valuable password, answer NO."),
		form, 3,
		MSG_BOX_BUTTON(N_("~Yes"), remember_form, B_ENTER),
		MSG_BOX_BUTTON(N_("~No"), dont_remember_form, B_ESC),
		MSG_BOX_BUTTON(N_("Ne~ver for this site"), never_for_this_site, 0));

	return;

fail:
	done_formhist_item(form);
}

static void
done_form_history(struct module *module)
{
	struct formhist_data *form, *next;

	foreachsafe (form, next, saved_forms) {
		delete_formhist_item(form);
	}
}

struct module forms_history_module = struct_module(
	/* name: */		N_("Form History"),
	/* options: */		forms_history_options,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		done_form_history
);
