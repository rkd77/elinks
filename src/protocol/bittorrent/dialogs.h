
#ifndef EL__PROTOCOL_BITTORRENT_DIALOG_H
#define EL__PROTOCOL_BITTORRENT_DIALOG_H

#include "bfu/common.h"

struct color_pair;
struct bittorrent_connection;
struct download;
struct session;
struct terminal;
struct type_query;

unsigned char *
get_bittorrent_message(struct download *download, struct terminal *term,
		       int wide, int full, unsigned char *separator);

void
draw_bittorrent_piece_progress(struct download *download, struct terminal *term,
			       int x, int y, int width, unsigned char *text,
			       struct color_pair *meter_color);

void set_bittorrent_files_for_deletion(struct download *download);
void set_bittorrent_notify_on_completion(struct download *download, struct terminal *term);

void notify_bittorrent_download_complete(struct bittorrent_connection *bittorrent);

widget_handler_status_T
dlg_show_bittorrent_info(struct dialog_data *dlg_data, struct widget_data *widget_data);

void bittorrent_message_dialog(struct session *ses, void *data);

void query_bittorrent_dialog(struct type_query *type_query);

#endif
