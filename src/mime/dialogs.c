/* Internal MIME types implementation dialogs */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/menu.h"
#include "config/options.h"
#include "mime/dialogs.h"
#include "intl/gettext/libintl.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/memory.h"
#include "util/string.h"


static struct option *
get_real_opt(unsigned char *base, unsigned char *id)
{
	struct option *opt = get_opt_rec_real(config_options, base);
	struct string translated;

	assert(opt);

	if (init_string(&translated)
	    && add_optname_to_string(&translated, id, strlen(id)))
		opt = get_opt_rec_real(opt, translated.source);

	done_string(&translated);

	return opt;
}


static void
really_del_ext(void *fcp)
{
	struct option *opt = get_real_opt("mime.extension",
					  (unsigned char *) fcp);

	if (opt) delete_option(opt);
}

void
menu_del_ext(struct terminal *term, void *fcp, void *xxx2)
{
	/* [gettext_accelerator_context(menu_del_ext)] */
	struct option *opt = NULL;
	unsigned char *extension = fcp;

	if (!extension) return;

	opt = get_real_opt("mime.extension", extension);
	if (!opt) {
		mem_free(extension);
		return;
	}

	msg_box(term, getml(extension, (void *) NULL), MSGBOX_FREE_TEXT,
		N_("Delete extension"), ALIGN_CENTER,
		msg_text(term, N_("Delete extension %s -> %s?"),
			 extension, opt->value.string),
		extension, 2,
		MSG_BOX_BUTTON(N_("~Yes"), really_del_ext, B_ENTER),
		MSG_BOX_BUTTON(N_("~No"), NULL, B_ESC));
}


struct extension {
	unsigned char ext_orig[MAX_STR_LEN];
	unsigned char ext[MAX_STR_LEN];
	unsigned char ct[MAX_STR_LEN];
};

static void
add_mime_extension(void *data)
{
	struct extension *ext = data;
	struct string name;

	if (!ext || !init_string(&name)) return;

	add_to_string(&name, "mime.extension.");
	add_optname_to_string(&name, ext->ext, strlen(ext->ext));

	really_del_ext(ext->ext_orig); /* ..or rename ;) */
	safe_strncpy(get_opt_str(name.source), ext->ct, MAX_STR_LEN);
	option_changed(NULL, get_opt_rec(config_options, name.source));
	done_string(&name);
}

void
menu_add_ext(struct terminal *term, void *fcp, void *xxx2)
{
	/* [gettext_accelerator_context(menu_add_ext)] */
	struct extension *new;
	struct dialog *dlg;

#define MIME_WIDGETS_COUNT 4
	dlg = calloc_dialog(MIME_WIDGETS_COUNT, sizeof(*new));
	if (!dlg) {
		mem_free_if(fcp);
		return;
	}

	new = (struct extension *) get_dialog_offset(dlg, MIME_WIDGETS_COUNT);

	if (fcp) {
		struct option *opt = get_real_opt("mime.extension", fcp);

		if (opt) {
			safe_strncpy(new->ext, fcp, MAX_STR_LEN);
			safe_strncpy(new->ct, opt->value.string, MAX_STR_LEN);
			safe_strncpy(new->ext_orig, fcp, MAX_STR_LEN);
		}

		mem_free(fcp);
	}

	dlg->title = _("Extension", term);
	dlg->layouter = generic_dialog_layouter;

	add_dlg_field(dlg, _("Extension(s)", term), 0, 0, check_nonempty, MAX_STR_LEN, new->ext, NULL);
	add_dlg_field(dlg, _("Content-Type", term), 0, 0, check_nonempty, MAX_STR_LEN, new->ct, NULL);

	add_dlg_ok_button(dlg, _("~OK", term), B_ENTER, add_mime_extension, new);
	add_dlg_button(dlg, _("~Cancel", term), B_ESC, cancel_dialog, NULL);

	add_dlg_end(dlg, MIME_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, (void *) NULL));
}


static struct menu_item mi_no_ext[] = {
	INIT_MENU_ITEM(N_("No extensions"), NULL, ACT_MAIN_NONE, NULL, NULL, NO_SELECT),
	NULL_MENU_ITEM
};

void
menu_list_ext(struct terminal *term, void *fn_, void *xxx)
{
	menu_func_T fn = fn_;
	LIST_OF(struct option) *opt_tree = get_opt_tree("mime.extension");
	struct option *opt;
	struct menu_item *mi = NULL;

	foreachback (opt, *opt_tree) {
		struct string translated;
		unsigned char *translated2;
		unsigned char *optptr2;

		if (!strcmp(opt->name, "_template_")) continue;

		if (!init_string(&translated)
		    || !add_real_optname_to_string(&translated, opt->name,
						   strlen(opt->name))) {
			done_string(&translated);
			continue;
		}

		if (!mi) {
			mi = new_menu(FREE_ANY | NO_INTL);
			if (!mi) {
				done_string(&translated);
				return;
			}
		}

		translated2 = memacpy(translated.source, translated.length);
		optptr2 = stracpy(opt->value.string);

		if (translated2 && optptr2) {
			add_to_menu(&mi, translated.source, optptr2, ACT_MAIN_NONE,
				    fn, translated2, 0);
		} else {
			mem_free_if(optptr2);
			mem_free_if(translated2);
			done_string(&translated);
		}
	}

	if (!mi) mi = mi_no_ext;
	do_menu(term, mi, NULL, 0);
}
