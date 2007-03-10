/* Prefabricated message box implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/button.h"
#include "bfu/msgbox.h"
#include "bfu/text.h"
#include "intl/gettext/libintl.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"


struct dialog_data *
msg_box(struct terminal *term, struct memory_list *ml, enum msgbox_flags flags,
	unsigned char *title, enum format_align align,
	unsigned char *text, void *udata, int buttons, ...)
{
	struct dialog *dlg;
	va_list ap;

	/* Check if the info string is valid. */
	if (!text || buttons < 0) return NULL;

	/* Use the @flags to determine whether @text should be free()d. */
	if (flags & MSGBOX_FREE_TEXT)
		add_one_to_ml(&ml, text);

	/* Use the @flags to determine whether strings should be l18n'd. */
	if (!(flags & MSGBOX_NO_INTL)) {
		title = _(title, term);
		if (!(flags & MSGBOX_FREE_TEXT)
		    && !(flags & MSGBOX_NO_TEXT_INTL))
			text = _(text, term);
		/* Button labels will be gettextized as will they be extracted
		 * from @ap. */
	}

	dlg = calloc_dialog(buttons + 1, 0);
	if (!dlg) {
		freeml(ml);
		return NULL;
	}

	add_one_to_ml(&ml, dlg);

	dlg->title = title;
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.padding_top = 1;
	dlg->udata2 = udata;

	if (flags & MSGBOX_SCROLLABLE)
		dlg->widgets->info.text.is_scrollable = 1;
	add_dlg_text(dlg, text, align, 0);

	va_start(ap, buttons);

	while (dlg->number_of_widgets < buttons + 1) {
		unsigned char *label;
		done_handler_T *done;
		int bflags;

		label = va_arg(ap, unsigned char *);
		done = va_arg(ap, done_handler_T *);
		bflags = va_arg(ap, int);

		if (!label) {
			/* Skip this button. */
			buttons--;
			continue;
		}

		if (!(flags & MSGBOX_NO_INTL))
			label = _(label, term);

		add_dlg_ok_button(dlg, label, bflags, done, udata);
	}

	va_end(ap);

	add_dlg_end(dlg, buttons + 1);

	return do_dialog(term, dlg, ml);
}

static inline unsigned char *
msg_text_do(unsigned char *format, va_list ap)
{
	unsigned char *info;
	int infolen, len;
	va_list ap2;

	VA_COPY(ap2, ap);

	infolen = vsnprintf(NULL, 0, format, ap2);
	info = mem_alloc(infolen + 1);
	if (!info) return NULL;

	len = vsnprintf((char *) info, infolen + 1, format, ap);
	if (len != infolen) {
		mem_free(info);
		return NULL;
	}

	/* Wear safety boots */
	info[infolen] = '\0';
	return info;
}

unsigned char *
msg_text(struct terminal *term, unsigned char *format, ...)
{
	unsigned char *info;
	va_list ap;

	va_start(ap, format);
	info = msg_text_do(_(format, term), ap);
	va_end(ap);

	return info;
}

static void
abort_refreshed_msg_box_handler(struct dialog_data *dlg_data)
{
	void *data = dlg_data->dlg->widgets->text;

	if (dlg_data->dlg->udata != data)
		mem_free(data);
}

static enum dlg_refresh_code
refresh_msg_box(struct dialog_data *dlg_data, void *data)
{
	unsigned char *(*get_info)(struct terminal *, void *) = data;
	void *msg_data = dlg_data->dlg->udata2;
	unsigned char *info = get_info(dlg_data->win->term, msg_data);

	if (!info) return REFRESH_CANCEL;

	abort_refreshed_msg_box_handler(dlg_data);

	dlg_data->dlg->widgets->text = info;
	return REFRESH_DIALOG;
}

void
refreshed_msg_box(struct terminal *term, enum msgbox_flags flags,
		  unsigned char *title, enum format_align align,
		  unsigned char *(get_info)(struct terminal *, void *),
		  void *data)
{
	/* [gettext_accelerator_context(refreshed_msg_box)] */
	struct dialog_data *dlg_data;
	unsigned char *info = get_info(term, data);

	if (!info) return;

	dlg_data = msg_box(term, NULL, flags | MSGBOX_FREE_TEXT,
			   title, align,
			   info,
			   data, 1,
			   MSG_BOX_BUTTON(N_("~OK"), NULL, B_ENTER | B_ESC));

	if (!dlg_data) return;

	/* Save the original text to check up on it when the dialog
	 * is freed. */
	dlg_data->dlg->udata = info;
	dlg_data->dlg->abort = abort_refreshed_msg_box_handler;
	refresh_dialog(dlg_data, refresh_msg_box, get_info);
}

struct dialog_data *
info_box(struct terminal *term, enum msgbox_flags flags,
	 unsigned char *title, enum format_align align,
	 unsigned char *text)
{
	/* [gettext_accelerator_context(info_box)] */
	return msg_box(term, NULL, flags,
		       title, align,
		       text,
		       NULL, 1,
		       MSG_BOX_BUTTON(N_("~OK"), NULL, B_ENTER | B_ESC));
}
