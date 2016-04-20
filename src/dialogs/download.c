/* Download dialogs */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/hierbox.h"
#include "dialogs/download.h"
#include "dialogs/menu.h"
#include "dialogs/progress.h"
#include "dialogs/status.h"
#include "intl/gettext/libintl.h"
#include "main/object.h"
#include "main/select.h"
#include "network/connection.h"
#include "network/progress.h"
#include "protocol/bittorrent/dialogs.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/time.h"


static void
undisplay_download(struct file_download *file_download)
{
	/* We are maybe called from bottom halve so check consistency */
	if (is_in_downloads_list(file_download) && file_download->dlg_data)
		cancel_dialog(file_download->dlg_data, NULL);
}

static void
do_abort_download(struct file_download *file_download)
{
	/* We are maybe called from bottom halve so check consistency */
	if (is_in_downloads_list(file_download)) {
		file_download->stop = 1;
		abort_download(file_download);
	}
}

static widget_handler_status_T
dlg_set_notify(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	file_download->notify = 1;
	/* The user of this terminal wants to be notified about the
	 * download.  Make this also the terminal where the
	 * notification appears.  However, keep the original terminal
	 * for external handlers, because the handler may have been
	 * chosen based on the environment variables (usually TERM or
	 * DISPLAY) of the ELinks process in that terminal.  */
	if (!file_download->external_handler)
		file_download->term = dlg_data->win->term;

#if CONFIG_BITTORRENT
	if (file_download->uri->protocol == PROTOCOL_BITTORRENT)
		set_bittorrent_notify_on_completion(&file_download->download,
						    file_download->term);
#endif
	undisplay_download(file_download);
	return EVENT_PROCESSED;
}

static widget_handler_status_T
dlg_abort_download(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	object_unlock(file_download);
	register_bottom_half(do_abort_download, file_download);
	return EVENT_PROCESSED;
}

static widget_handler_status_T
push_delete_button(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	file_download->delete_ = 1;
#if CONFIG_BITTORRENT
	if (file_download->uri->protocol == PROTOCOL_BITTORRENT)
		set_bittorrent_files_for_deletion(&file_download->download);
#endif
	object_unlock(file_download);
	register_bottom_half(do_abort_download, file_download);
	return EVENT_PROCESSED;
}

static widget_handler_status_T
dlg_undisplay_download(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	object_unlock(file_download);
	register_bottom_half(undisplay_download, file_download);
	return EVENT_PROCESSED;
}


static void
download_abort_function(struct dialog_data *dlg_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;

	file_download->dlg_data = NULL;
}


static void
download_dialog_layouter(struct dialog_data *dlg_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;
	struct terminal *term = dlg_data->win->term;
	int w = dialog_max_width(term);
	int rw = w;
	int x, y = 0;
	int url_len;
	unsigned char *url;
	struct download *download = &file_download->download;
	struct color_pair *dialog_text_color = get_bfu_color(term, "dialog.text");
	unsigned char *msg = get_download_msg(download, term, 1, 1, "\n");
	int show_meter = (download_is_progressing(download)
			  && download->progress->size >= 0);
#if CONFIG_BITTORRENT
	int bittorrent = (file_download->uri->protocol == PROTOCOL_BITTORRENT
			  && (show_meter || is_in_state(download->state, S_RESUME)));
#endif

	redraw_windows(REDRAW_BEHIND_WINDOW, dlg_data->win);
	file_download->dlg_data = dlg_data;

	if (!msg) return;

	url = get_uri_string(file_download->uri, URI_PUBLIC);
	if (!url) {
		mem_free(msg);
		return;
	}
#ifdef CONFIG_UTF8
	if (term->utf8_cp)
		decode_uri(url);
	else
#endif /* CONFIG_UTF8 */
		decode_uri_for_display(url);
	url_len = strlen(url);

	if (show_meter) {
		int_lower_bound(&w, DOWN_DLG_MIN);
	}

	dlg_format_text_do(dlg_data, url, 0, &y, w, &rw,
			dialog_text_color, ALIGN_LEFT, 1);

	y++;
	if (show_meter) y += 2;

#if CONFIG_BITTORRENT
	if (bittorrent) y += 2;
#endif
	dlg_format_text_do(dlg_data, msg, 0, &y, w, &rw,
			dialog_text_color, ALIGN_LEFT, 1);

	y++;
	dlg_format_buttons(dlg_data, dlg_data->widgets_data,
			   dlg_data->number_of_widgets, 0, &y, w,
			   &rw, ALIGN_CENTER, 1);

	draw_dialog(dlg_data, w, y);

	w = rw;
	if (url_len > w) {
		/* Truncate too long urls */
		url_len = w;
		url[url_len] = '\0';
		if (url_len > 4) {
			url[--url_len] = '.';
			url[--url_len] = '.';
			url[--url_len] = '.';
		}
	}

	y = dlg_data->box.y + DIALOG_TB + 1;
	x = dlg_data->box.x + DIALOG_LB;
	dlg_format_text_do(dlg_data, url, x, &y, w, NULL,
			dialog_text_color, ALIGN_LEFT, 0);

	if (show_meter) {
		y++;
		draw_progress_bar(download->progress, term, x, y, w, NULL, NULL);
		y++;
	}

#if CONFIG_BITTORRENT
	if (bittorrent) {
		y++;
		draw_bittorrent_piece_progress(download, term, x, y, w, NULL, NULL);
		y++;
	}
#endif
	y++;
	dlg_format_text_do(dlg_data, msg, x, &y, w, NULL,
			dialog_text_color, ALIGN_LEFT, 0);

	y++;
	dlg_format_buttons(dlg_data, dlg_data->widgets_data,
			   dlg_data->number_of_widgets, x, &y, w,
			   NULL, ALIGN_CENTER, 0);

	mem_free(url);
	mem_free(msg);
}

void
display_download(struct terminal *term, struct file_download *file_download,
		 struct session *ses)
{
	/* [gettext_accelerator_context(display_download)] */
	struct dialog *dlg;

	if (!is_in_downloads_list(file_download))
		return;

#if CONFIG_BITTORRENT
#define DOWNLOAD_WIDGETS_COUNT 5
#else
#define DOWNLOAD_WIDGETS_COUNT 4
#endif

	dlg = calloc_dialog(DOWNLOAD_WIDGETS_COUNT, 0);
	if (!dlg) return;

	undisplay_download(file_download);
	file_download->ses = ses;
	dlg->title = _("Download", term);
	dlg->layouter = download_dialog_layouter;
	dlg->abort = download_abort_function;
	dlg->udata = file_download;

	object_lock(file_download);

	add_dlg_button(dlg, _("~Background", term), B_ENTER | B_ESC, dlg_undisplay_download, NULL);
	add_dlg_button(dlg, _("Background with ~notify", term), B_ENTER | B_ESC, dlg_set_notify, NULL);

#if CONFIG_BITTORRENT
	if (file_download->uri->protocol == PROTOCOL_BITTORRENT)
		add_dlg_button(dlg, _("~Info", term), B_ENTER | B_ESC, dlg_show_bittorrent_info, NULL);
#endif

	add_dlg_button(dlg, _("~Abort", term), 0, dlg_abort_download, NULL);

	/* Downloads scheduled to be opened by external handlers are always
	 * deleted. */
	if (!file_download->external_handler) {
		add_dlg_button(dlg, _("Abort and ~delete file", term), 0, push_delete_button, NULL);
	}

#if CONFIG_BITTORRENT
	add_dlg_end(dlg, DOWNLOAD_WIDGETS_COUNT - !!file_download->external_handler
		         - (file_download->uri->protocol != PROTOCOL_BITTORRENT));
#else
	add_dlg_end(dlg, DOWNLOAD_WIDGETS_COUNT - !!file_download->external_handler);
#endif

	do_dialog(term, dlg, getml(dlg, (void *) NULL));
}


/* The download manager */

static void
lock_file_download(struct listbox_item *item)
{
	object_lock((struct file_download *) item->udata);
}

static void
unlock_file_download(struct listbox_item *item)
{
	object_unlock((struct file_download *) item->udata);
}

static int
is_file_download_used(struct listbox_item *item)
{
	return is_object_used((struct file_download *) item->udata);
}

static unsigned char *
get_file_download_text(struct listbox_item *item, struct terminal *term)
{
	struct file_download *file_download = item->udata;
	unsigned char *uristring;

	uristring = get_uri_string(file_download->uri, URI_PUBLIC);
	if (uristring) {
#ifdef CONFIG_UTF8
		if (term->utf8_cp)
			decode_uri(uristring);
		else
#endif /* CONFIG_UTF8 */
			decode_uri_for_display(uristring);
	}

	return uristring;
}

static unsigned char *
get_file_download_info(struct listbox_item *item, struct terminal *term)
{
	return NULL;
}

static struct uri *
get_file_download_uri(struct listbox_item *item)
{
	struct file_download *file_download = item->udata;

	return get_uri_reference(file_download->uri);
}

static struct listbox_item *
get_file_download_root(struct listbox_item *item)
{
	return NULL;
}

static int
can_delete_file_download(struct listbox_item *item)
{
	return 1;
}

static void
delete_file_download(struct listbox_item *item, int last)
{
	struct file_download *file_download = item->udata;

	assert(!is_object_used(file_download));
	register_bottom_half(do_abort_download, file_download);
}

static enum dlg_refresh_code
refresh_file_download(struct dialog_data *dlg_data, void *data)
{
	/* Always refresh (until we keep finished downloads) */
	return are_there_downloads() ? REFRESH_DIALOG : REFRESH_STOP;
}

/* TODO: Make it configurable */
#define DOWNLOAD_METER_WIDTH 15
#define DOWNLOAD_URI_PERCENTAGE 50

static void
draw_file_download(struct listbox_item *item, struct listbox_context *context,
		   int x, int y, int width)
{
	struct file_download *file_download = item->udata;
	struct download *download = &file_download->download;
	unsigned char *stylename;
	struct color_pair *color;
	unsigned char *text;
	int length;
	int trimmedlen;
	int meter = DOWNLOAD_METER_WIDTH;

	/* We have nothing to work with */
	if (width < 4) return;

	stylename = (item == context->box->sel) ? "menu.selected"
		  : ((item->marked)	        ? "menu.marked"
					        : "menu.normal");

	color = get_bfu_color(context->term, stylename);

	text = get_file_download_text(item, context->term);
	if (!text) return;

	length = strlen(text);
	/* Show atleast the required percentage of the URI */
	if (length * DOWNLOAD_URI_PERCENTAGE / 100 < width - meter - 4) {
		trimmedlen = int_min(length, width - meter - 4);
	} else {
		trimmedlen = int_min(length, width - 3);
	}

	draw_text(context->term, x, y, text, trimmedlen, 0, color);
	if (trimmedlen < length) {
		draw_text(context->term, x + trimmedlen, y, "...", 3, 0, color);
		trimmedlen += 3;
	}

	mem_free(text);

	if (!download->progress
	    || download->progress->size < 0
	    || !is_in_state(download->state, S_TRANS)
	    || !has_progress(download->progress)) {
		/* TODO: Show trimmed error message. */
		return;
	}

	if (!dialog_has_refresh(context->dlg_data))
		refresh_dialog(context->dlg_data, refresh_file_download, NULL);

	if (trimmedlen + meter >= width) return;

	x += width - meter;

	draw_progress_bar(download->progress, context->term, x, y, meter, NULL, NULL);
}

static struct listbox_ops_messages download_messages = {
	/* cant_delete_item */
	N_("Sorry, but download \"%s\" cannot be interrupted."),
	/* cant_delete_used_item */
	N_("Sorry, but download \"%s\" is being used by something else."),
	/* cant_delete_folder */
	NULL,
	/* cant_delete_used_folder */
	NULL,
	/* delete_marked_items_title */
	N_("Interrupt marked downloads"),
	/* delete_marked_items */
	N_("Interrupt marked downloads?"),
	/* delete_folder_title */
	NULL,
	/* delete_folder */
	NULL,
	/* delete_item_title */
	N_("Interrupt download"),
	/* delete_item; xgettext:c-format */
	N_("Interrupt this download?"),
	/* clear_all_items_title */
	N_("Interrupt all downloads"),
	/* clear_all_items_title */
	N_("Do you really want to interrupt all downloads?"),
};

static const struct listbox_ops downloads_listbox_ops = {
	lock_file_download,
	unlock_file_download,
	is_file_download_used,
	get_file_download_text,
	get_file_download_info,
	get_file_download_uri,
	get_file_download_root,
	NULL,
	can_delete_file_download,
	delete_file_download,
	draw_file_download,
	&download_messages,
};


static widget_handler_status_T
push_info_button(struct dialog_data *dlg_data, struct widget_data *button)
{
	struct listbox_data *box = get_dlg_listbox_data(dlg_data);
	struct terminal *term = dlg_data->win->term;
	struct session *ses = dlg_data->dlg->udata;
	struct file_download *file_download = box->sel ? box->sel->udata : NULL;

	assert(ses);

	if (!file_download) return EVENT_PROCESSED;

	/* Don't layer on top of the download manager */
	delete_window(dlg_data->win);

	display_download(term, file_download, ses);
	return EVENT_PROCESSED;
}


/* TODO: Ideas for buttons .. should be pretty trivial most of it
 *
 * - Resume or something that will use some goto like handler
 * - Open button that can be used to set file_download->prog.
 * - Toggle notify button
 */
static const struct hierbox_browser_button download_buttons[] = {
	/* [gettext_accelerator_context(.download_buttons)] */
	{ N_("~Info"),                  push_info_button           },
	{ N_("~Abort"),                 push_hierbox_delete_button },
#if 0
	/* This requires more work to make locking work and query the user */
	{ N_("Abort and delete file"),  push_delete_button         },
#endif
	{ N_("C~lear"),                 push_hierbox_clear_button  },
};

static struct_hierbox_browser(
	download_browser,
	N_("Download manager"),
	download_buttons,
	&downloads_listbox_ops
);

void
download_manager(struct session *ses)
{
	hierbox_browser(&download_browser, ses);

	/* FIXME: It's workaround for bug 397. Real fix is needed. */
	download_browser.do_not_save_state = 1;
}

void
init_download_display(struct file_download *file_download)
{
	file_download->box_item = add_listbox_leaf(&download_browser, NULL,
						   file_download);
}

void
done_download_display(struct file_download *file_download)
{
	if (file_download->box_item) {
		done_listbox_item(&download_browser, file_download->box_item);
		file_download->box_item = NULL;
	}
}

