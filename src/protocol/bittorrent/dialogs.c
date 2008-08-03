/* BitTorrent specific dialogs */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "dialogs/document.h"
#include "dialogs/download.h"
#include "dialogs/progress.h"
#include "intl/gettext/libintl.h"
#include "mime/mime.h"
#include "network/connection.h"
#include "network/state.h"
#include "protocol/bittorrent/bencoding.h"
#include "protocol/bittorrent/bittorrent.h"
#include "protocol/bittorrent/common.h"
#include "protocol/bittorrent/piececache.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "util/conv.h"
#include "util/string.h"


struct bittorrent_download_info {
	LIST_OF(struct string_list_item) labels;
	unsigned char *name;
	int *selection;
	size_t size;
};

static void
done_bittorrent_download_info(struct bittorrent_download_info *info)
{
	free_string_list(&info->labels);
	mem_free_if(info->selection);
	mem_free_if(info->name);
	mem_free(info);
}

static struct bittorrent_download_info *
init_bittorrent_download_info(struct bittorrent_meta *meta)
{
	struct bittorrent_download_info *info;
	struct bittorrent_file *file;
	size_t max_label = 0;

	info = mem_calloc(1, sizeof(*info));
	if (!info) return NULL;

	init_list(info->labels);

	info->name = stracpy(meta->name);
	if (!info->name) {
		mem_free(info);
		return NULL;
	}

	foreach (file, meta->files) {
		struct string string;
		int spaces;

		if (!init_string(&string))
			break;

		info->size++;

		add_xnum_to_string(&string, file->length);

		/*   40 K   file1
		 * 2300 MiB file2 */
		spaces = string.length - strcspn(string.source, " ") - 1;
		add_xchar_to_string(&string, ' ', int_max(4 - spaces, 1));

		add_to_string_list(&info->labels, string.source, string.length);
		if (string.length > max_label)
			max_label = string.length;

		done_string(&string);
	}

	info->selection = mem_calloc(info->size, sizeof(*info->selection));
	if (!info->selection || info->size != list_size(&meta->files)) {
		done_bittorrent_download_info(info);
		return NULL;
	}

	info->size = 0;

	foreach (file, meta->files) {
		struct string_list_item *item;
		size_t pos = 0;

		foreach (item, info->labels)
			if (pos++ == info->size)
				break;

		info->selection[info->size++] = 1;
		pos = max_label - item->string.length;

		add_to_string(&item->string, file->name);

		for (; pos > 0; pos--) {
			insert_in_string(&item->string.source, 0, " ", 1);
		}
	}

	return info;
}


/* Add information from the meta file struct to a string. */
static void
add_bittorrent_meta_to_string(struct string *msg, struct bittorrent_meta *meta,
			      struct terminal *term, int add_files)
{
	if (meta->malicious_paths) {
		add_format_to_string(msg, "%s\n\n",
			_("Warning: potential malicious path detected", term));
	}

	add_format_to_string(msg, "\n%s: ",
		_("Size", term));
	add_xnum_to_string(msg,
		(off_t) (meta->pieces - 1) * meta->piece_length
		+ meta->last_piece_length);

	if (meta->last_piece_length == meta->piece_length) {
		add_format_to_string(msg, " (%ld * %ld)",
			meta->pieces, meta->piece_length);
	} else {
		add_format_to_string(msg, " (%ld * %ld + %ld)",
			meta->pieces - 1, meta->piece_length,
			meta->last_piece_length);
	}

	add_format_to_string(msg, "\n%s: %s",
		_("Info hash", term),
		get_hexed_bittorrent_id(meta->info_hash));

	add_format_to_string(msg, "\n%s: %s",
		_("Announce URI", term),
		struri(meta->tracker_uris.uris[0]));

#ifdef HAVE_STRFTIME
	if (meta->creation_date) {
		add_format_to_string(msg, "\n%s: ",
			_("Creation date", term));
		add_date_to_string(msg,
			get_opt_str("ui.date_format"),
			&meta->creation_date);
	}
#endif

	if (meta->type == BITTORRENT_MULTI_FILE) {
		add_format_to_string(msg, "\n%s: %s",
			_("Directory", term), meta->name);
	}

	if (add_files) {
		struct bittorrent_download_info *info;
		struct string_list_item *item;

		info = init_bittorrent_download_info(meta);
		if (info) {
			add_format_to_string(msg, "\n%s:",
				_("Files", term));

			foreach (item, info->labels) {
				add_format_to_string(msg, "\n  %s",
					item->string.source);
			}

			done_bittorrent_download_info(info);
		}
	}

	if (meta->comment && *meta->comment) {
		add_format_to_string(msg, "\n%s:\n %s",
		_("Comment", term), meta->comment);
	}
}


/* ************************************************************************** */
/* Download dialog button hooks and helpers: */
/* ************************************************************************** */

void
set_bittorrent_files_for_deletion(struct download *download)
{
	struct bittorrent_connection *bittorrent;

	if (!download->conn || !download->conn->info)
		return;

	bittorrent = download->conn->info;
	bittorrent->cache->delete_files = 1;
}

void
set_bittorrent_notify_on_completion(struct download *download, struct terminal *term)
{
	struct bittorrent_connection *bittorrent;

	if (!download->conn || !download->conn->info)
		return;

	bittorrent = download->conn->info;
	bittorrent->cache->notify_complete = 1;
	bittorrent->term = term;
}

void
notify_bittorrent_download_complete(struct bittorrent_connection *bittorrent)
{
	struct connection *conn = bittorrent->conn;
	unsigned char *url = get_uri_string(conn->uri, URI_PUBLIC);

	if (!url) return;

	assert(bittorrent->term);

	info_box(bittorrent->term, MSGBOX_FREE_TEXT,
		N_("Download"), ALIGN_CENTER,
		msg_text(bittorrent->term, N_("Download complete:\n%s"), url));
	mem_free(url);
}

/* Handler for the Info-button available in the download dialog. */
widget_handler_status_T
dlg_show_bittorrent_info(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct file_download *file_download = dlg_data->dlg->udata;
	struct download *download = &file_download->download;
	struct string msg;

	if (download->conn
	    && download->conn->info
	    && init_string(&msg)) {
		struct terminal *term = file_download->term;
		struct bittorrent_connection *bittorrent;
		enum msgbox_flags flags = MSGBOX_FREE_TEXT;

		bittorrent = download->conn->info;

		add_bittorrent_meta_to_string(&msg, &bittorrent->meta, term, 1);

		if (list_size(&bittorrent->meta.files) > 1)
			flags |= MSGBOX_SCROLLABLE;

		info_box(term, flags,
			 N_("Download info"), ALIGN_LEFT, msg.source);
	}

	return EVENT_PROCESSED;
}


/* ************************************************************************** */
/* Download status message creation: */
/* ************************************************************************** */

/* Compose and return the status message about current download speed and
 * BitTorrent swarm info. It is called fairly often so most values used in here
 * should be easily accessible. */
unsigned char *
get_bittorrent_message(struct download *download, struct terminal *term,
		       int wide, int full, unsigned char *separator)
{
	/* Cooresponds to the connection mode enum. */
	static unsigned char *modes_text[] = {
		N_("downloading (random)"),
		N_("downloading (rarest first)"),
		N_("downloading (end game)"),
		N_("seeding"),
	};
	struct bittorrent_connection *bittorrent;
	struct bittorrent_peer_connection *peer;
	struct string string;
	unsigned char *msg;
	uint32_t value;

	if (!download->conn
	    || !download->conn->info
	    || !init_string(&string))
		return NULL;

	/* Get the download speed information message. */
	msg = get_progress_msg(download->progress, term, wide, full, separator);
	if (!msg) {
		done_string(&string);
		return NULL;
	}

	bittorrent = download->conn->info;

	add_to_string(&string, msg);
	mem_free(msg);

	/* Status: */

	add_format_to_string(&string, "\n\n%s: %s",
		_("Status", term), _(modes_text[bittorrent->mode], term));

	if (bittorrent->cache->partial)
		add_format_to_string(&string, " (%s)",
			_("partial", term));

	/* Peers: */

	add_format_to_string(&string, "\n%s: ", _("Peers", term));

	value = list_size(&bittorrent->peers);
	add_format_to_string(&string,
		n_("%u connection", "%u connections", value, term), value);

	add_to_string(&string, ", ");

	value = 0;
	foreach (peer, bittorrent->peers)
		if (peer->remote.seeder)
			value++;

	add_format_to_string(&string,
		n_("%u seeder", "%u seeders", value, term), value);

	add_to_string(&string, ", ");

	value = list_size(&bittorrent->peer_pool);
	add_format_to_string(&string,
		n_("%u available", "%u available", value, term), value);

	/* Swarm info: */

	if (bittorrent->complete > 0 || bittorrent->incomplete > 0) {
		add_format_to_string(&string, "\n%s: ", _("Swarm info", term));

		if (bittorrent->complete > 0) {
			value = bittorrent->complete;
			add_format_to_string(&string,
				n_("%u seeder", "%u seeders", value, term), value);
		}

		if (bittorrent->incomplete > 0) {
			if (bittorrent->complete > 0)
				add_to_string(&string, ", ");
			value = bittorrent->incomplete;
			add_format_to_string(&string,
				n_("%u downloader", "%u downloaders", value, term), value);
		}
	}

	/* Upload: */

	add_format_to_string(&string, "\n%s: ", _("Upload", term));
	add_xnum_to_string(&string, bittorrent->uploaded);
	add_to_string(&string, ", ");

	add_xnum_to_string(&string, bittorrent->upload_progress.current_speed);
	add_to_string(&string, "/s, ");

	add_xnum_to_string(&string, bittorrent->upload_progress.average_speed);
	add_format_to_string(&string, "/s %s", _("average", term));

	if (bittorrent->uploaded < bittorrent->downloaded) {
		struct progress *progress = &bittorrent->upload_progress;

		add_format_to_string(&string, ", %s ", _("1:1 in", term));
		add_timeval_to_string(&string, &progress->estimated_time);
	}

	/* Sharing: */

	add_format_to_string(&string, "\n%s: ", _("Sharing", term));
	if (bittorrent->downloaded) {
		add_format_to_string(&string, "%.3f", bittorrent->sharing_rate);
	} else {
		/* Idea taken from btdownloadcurses .. 'oo' means infinite. */
		add_to_string(&string, "oo");
	}
	add_to_string(&string, " (");
	add_xnum_to_string(&string, bittorrent->uploaded);
	add_format_to_string(&string, " %s / ", _("uploaded", term));
	add_xnum_to_string(&string, bittorrent->downloaded);
	add_format_to_string(&string, " %s)", _("downloaded", term));

	/* Pieces: */

	add_format_to_string(&string, "\n%s: ", _("Pieces", term));

	value = bittorrent->cache->completed_pieces;
	add_format_to_string(&string,
		n_("%u completed", "%u completed", value, term), value);

	value = bittorrent->cache->loading_pieces;
	if (value) {
		add_to_string(&string, ", ");
		add_format_to_string(&string,
			n_("%u in progress", "%u in progress", value, term), value);
	}

	if (bittorrent->cache->partial)
		value = bittorrent->cache->partial_pieces;
	else
		value = bittorrent->meta.pieces;
	value -= bittorrent->cache->completed_pieces;
	if (value) {
		add_to_string(&string, ", ");
		add_format_to_string(&string,
			n_("%u remaining", "%u remaining", value, term), value);
	}

	/* Statistics: */

	add_format_to_string(&string, "\n%s: ", _("Statistics", term));

	value = list_size(&bittorrent->cache->queue);
	add_format_to_string(&string,
		n_("%u in memory", "%u in memory", value, term), value);

	value = bittorrent->cache->locked_pieces;
	if (value) {
		add_to_string(&string, ", ");
		add_format_to_string(&string,
			n_("%u locked", "%u locked", value, term), value);
	}

	value = bittorrent->cache->rejected_pieces;
	if (value) {
		add_to_string(&string, ", ");
		add_format_to_string(&string,
			n_("%u rejected", "%u rejected", value, term), value);
	}

	value = bittorrent->cache->unavailable_pieces;
	if (value) {
		add_to_string(&string, ", ");
		add_format_to_string(&string,
			n_("%u unavailable", "%u unavailable", value, term), value);
	}

	return string.source;
}

void
draw_bittorrent_piece_progress(struct download *download, struct terminal *term,
			       int x, int y, int width, unsigned char *text,
			       struct color_pair *color)
{
	struct bittorrent_connection *bittorrent;
	uint32_t piece;
	int x_start;

	if (!download->conn || !download->conn->info)
		return;

	bittorrent = download->conn->info;

	/* Draw the progress meter part "[###    ]" */
	if (!text && width > 2) {
		width -= 2;
		draw_text(term, x++, y, "[", 1, 0, NULL);
		draw_text(term, x + width, y, "]", 1, 0, NULL);
	}

	x_start = x;

	if (width <= 0 || !bittorrent->cache)
		return;

	if (!color) color = get_bfu_color(term, "dialog.meter");

	if (bittorrent->meta.pieces <= width) {
		int chars_per_piece = width / bittorrent->meta.pieces;
		int remainder	    = width % bittorrent->meta.pieces;

		for (piece = 0; piece < bittorrent->meta.pieces; piece++) {
			struct box piecebox;

			set_box(&piecebox, x, y, chars_per_piece + !!remainder, 1);

			if (bittorrent->cache->entries[piece].completed)
				draw_box(term, &piecebox, ' ', 0, color);

			x += chars_per_piece + !!remainder;
			if (remainder > 0) remainder--;
		}

	} else {
		int pieces_per_char = bittorrent->meta.pieces / width;
		int remainder 	    = bittorrent->meta.pieces % width;
		struct color_pair inverted;
		uint32_t completed = 0, remaining = 0;
		int steps = pieces_per_char + !!remainder;

		inverted.background = color->foreground;
		inverted.foreground = color->background;

		for (piece = 0; piece < bittorrent->meta.pieces; piece++) {
			if (bittorrent->cache->entries[piece].completed)
				completed++;
			else
				remaining++;

			if (--steps > 0)
				continue;

			assert(completed <= pieces_per_char + !!remainder);
			assert(remaining <= pieces_per_char + !!remainder);

			if (!remaining)				/*  100% */
				draw_char_color(term, x, y, color);

			else if (completed > remaining)		/* > 50% */
				draw_char(term, x, y, BORDER_SVLINE,
					  SCREEN_ATTR_FRAME, color);

			else if (completed)			/* >  0% */
				draw_char(term, x, y, BORDER_SVLINE,
					  SCREEN_ATTR_FRAME, &inverted);

			x++;
			if (remainder > 0) remainder--;

			remaining = completed = 0;
			steps = pieces_per_char + !!remainder;
		}
	}

	if (is_in_state(download->state, S_RESUME)) {
		static unsigned char s[] = "????"; /* Reduce or enlarge at will. */
		unsigned int slen = 0;
		int max = int_min(sizeof(s), width) - 1;
		int percent = 0;

		percent = (int) ((longlong) 100 * bittorrent->cache->resume_pos
						/ bittorrent->meta.pieces);

		if (ulongcat(s, &slen, percent, max, 0)) {
			s[0] = '?';
			slen = 1;
		}

		s[slen++] = '%';

		/* Draw the percentage centered in the progress meter */
		x_start += (1 + width - slen) / 2;

		assert(slen <= width);

		draw_text(term, x_start, y, s, slen, 0, NULL);
	}
}


/* ************************************************************************** */
/* Display Failure Reason from the tracker: */
/* ************************************************************************** */

void
bittorrent_message_dialog(struct session *ses, void *data)
{
	struct bittorrent_message *message = data;
	struct string string;
	unsigned char *uristring;

	/* Don't show error dialogs for missing CSS stylesheets */
	if (!init_string(&string))
		return;

	uristring = get_uri_string(message->uri, URI_PUBLIC);
	if (uristring) {
#ifdef CONFIG_UTF8
		if (ses->tab->term->utf8_cp)
			decode_uri(uristring);
		else
#endif /* CONFIG_UTF8 */
			decode_uri_for_display(uristring);
		add_format_to_string(&string,
			_("Unable to retrieve %s", ses->tab->term),
			uristring);
		mem_free(uristring);
		add_to_string(&string, ":\n\n");
	}

	if (!is_in_state(message->state, S_OK)) {
		add_format_to_string(&string, "%s: %s",
			get_state_message(connection_state(S_BITTORRENT_TRACKER),
					  ses->tab->term),
			get_state_message(message->state, ses->tab->term));
	} else {
		add_to_string(&string, message->string);
	}

	info_box(ses->tab->term, MSGBOX_FREE_TEXT,
		 N_("Error"), ALIGN_CENTER,
		 string.source);

	done_bittorrent_message(message);
}


/* ************************************************************************** */
/* BitTorrent download querying: */
/* ************************************************************************** */

static void
abort_bittorrent_download_query(struct dialog_data *dlg_data)
{
	struct bittorrent_download_info *info = dlg_data->dlg->udata;

	done_bittorrent_download_info(info);
}

/* The download button handler. Basicly it redirects <uri> to bittorrent:<uri>
 * and starts displaying the download. */
static widget_handler_status_T
bittorrent_download(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct type_query *type_query = dlg_data->dlg->udata2;
	struct bittorrent_download_info *info = dlg_data->dlg->udata;
	struct file_download *file_download;
	struct session *ses = type_query->ses;
	struct string redirect;
	struct uri *uri;
	struct connection conn;

	if (!init_string(&redirect))
		return cancel_dialog(dlg_data, widget_data);

	add_to_string(&redirect, "bittorrent:");
	add_uri_to_string(&redirect, type_query->uri, URI_ORIGINAL);

	uri = get_uri(redirect.source, 0);

	done_string(&redirect);
	tp_cancel(type_query);

	if (!uri)
		return cancel_dialog(dlg_data, widget_data);

	file_download = init_file_download(uri, ses, info->name, -1);
	done_uri(uri);

	if (!file_download)
		return cancel_dialog(dlg_data, widget_data);

	update_dialog_data(dlg_data);

	/* Put the meta info in the store. */
	add_bittorrent_selection(file_download->uri, info->selection, info->size);
	info->selection = NULL;
	info->name = NULL;

	/* XXX: Hackery to get the Info button installed */
	conn.uri = file_download->uri;
	file_download->download.conn = &conn;

	/* Done here and not in init_file_download() so that the external
	 * handler can become initialized. */
	display_download(ses->tab->term, file_download, ses);

	file_download->download.conn = NULL;

	load_uri(file_download->uri, ses->referrer, &file_download->download,
		 PRI_DOWNLOAD, CACHE_MODE_NORMAL, file_download->seek);

	return cancel_dialog(dlg_data, widget_data);
}


/* Show the protocol header. */
/* XXX: Code duplication with session/download.h */
widget_handler_status_T
tp_show_header(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct type_query *type_query = widget_data->widget->data;

	cached_header_dialog(type_query->ses, type_query->cached);

	return EVENT_PROCESSED;
}

/* Build a dialog querying the user on how to handle a .torrent file. */
static void
bittorrent_query_callback(void *data, struct connection_state state,
			    struct string *response)
{
	/* [gettext_accelerator_context(.bittorrent_query_callback)] */
	struct type_query *type_query = data;
	struct string filename;
	unsigned char *text;
	struct dialog *dlg;
#define BITTORRENT_QUERY_WIDGETS_COUNT	6
	int widgets = BITTORRENT_QUERY_WIDGETS_COUNT;
	struct terminal *term = type_query->ses->tab->term;
	struct bittorrent_download_info *info;
	struct bittorrent_meta meta;
	struct dialog_data *dlg_data;
	int selected_widget;
	struct memory_list *ml;
	struct string msg;
	int files;

	if (!is_in_state(state, S_OK))
		return;

	/* This should never happen, since setup_download_handler() should make
	 * sure to handle application/x-bittorrent document types in the default
	 * type query handler. */
	if (get_cmd_opt_bool("anonymous")) {
		INTERNAL("BitTorrent downloads not allowed in anonymous mode.");
		return;
	}

	if (!init_string(&msg))
		return;

	if (init_string(&filename)) {
		add_mime_filename_to_string(&filename, type_query->uri);

		/* Let's make the filename pretty for display & save */
		/* TODO: The filename can be the empty string here. See bug 396. */
#ifdef CONFIG_UTF8
		if (term->utf8_cp)
			decode_uri_string(&filename);
		else
#endif /* CONFIG_UTF8 */
			decode_uri_string_for_display(&filename);
	}

	add_format_to_string(&msg,
		_("What would you like to do with the file '%s'?", term),
		filename.source);

	done_string(&filename);

	if (parse_bittorrent_metafile(&meta, response) != BITTORRENT_STATE_OK) {
		print_error_dialog(type_query->ses,
				   connection_state(S_BITTORRENT_METAINFO),
				   type_query->uri, PRI_CANCEL);
		tp_cancel(type_query);
		done_string(&msg);
		return;
	}

	files = list_size(&meta.files);

	add_format_to_string(&msg, "\n%s:",
		_("Information about the torrent", term));

	add_bittorrent_meta_to_string(&msg, &meta, term, files == 1);

	info = init_bittorrent_download_info(&meta);
	done_bittorrent_meta(&meta);
	if (!info) {
		done_string(&msg);
		return;
	}

	dlg = calloc_dialog(widgets + files, msg.length + 1);
	if (!dlg) {
		done_bittorrent_download_info(info);
		done_string(&msg);
		return;
	}

	text = get_dialog_offset(dlg, widgets + files);
	memcpy(text, msg.source, msg.length + 1);
	done_string(&msg);

	dlg->title = _("What to do?", term);
	dlg->abort = abort_bittorrent_download_query;
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.padding_top = 1;
	dlg->layout.fit_datalen = 1;
	dlg->udata2 = type_query;
	dlg->udata = info;

	add_dlg_text(dlg, text, ALIGN_LEFT, 0);
	dlg->widgets->info.text.is_scrollable = 1;

	if (files > 1) {
		struct string_list_item *item;
		size_t index = 0;

		foreach (item, info->labels) {
			add_dlg_checkbox(dlg, item->string.source, &info->selection[index++]);
			widgets++;
		}
	}

	selected_widget = dlg->number_of_widgets;
	add_dlg_button(dlg, _("Down~load", term), B_ENTER,
			bittorrent_download, type_query);

	add_dlg_ok_button(dlg, _("Sa~ve", term), B_ENTER,
			  (done_handler_T *) tp_save, type_query);

	add_dlg_ok_button(dlg, _("~Display", term), B_ENTER,
			  (done_handler_T *) tp_display, type_query);

	if (type_query->cached && type_query->cached->head && *type_query->cached->head) {
		add_dlg_button(dlg, _("Show ~header", term), B_ENTER,
			       tp_show_header, type_query);
	} else {
		widgets--;
	}

	add_dlg_ok_button(dlg, _("~Cancel", term), B_ESC,
			  (done_handler_T *) tp_cancel, type_query);

	add_dlg_end(dlg, widgets);

	ml = getml(dlg, (void *) NULL);
	if (!ml) {
		/* XXX: Assume that the allocated @external_handler will be
		 * freed when releasing the @type_query. */
		done_bittorrent_download_info(info);
		mem_free(dlg);
		return;
	}

	dlg_data = do_dialog(term, dlg, ml);
	if (dlg_data)
		select_widget_by_id(dlg_data, selected_widget);
}

void
query_bittorrent_dialog(struct type_query *type_query)
{
	init_bittorrent_fetch(NULL, type_query->uri,
			      bittorrent_query_callback, type_query, 0);
}
