/* $Id: download.h,v 1.4 2004/01/07 03:18:19 jonas Exp $ */

#ifndef EL__DIALOGS_DOWNLOAD_H
#define EL__DIALOGS_DOWNLOAD_H

#include "bfu/hierbox.h"
#include "sched/download.h"
#include "sched/session.h"
#include "terminal/terminal.h"

void display_download(struct terminal *, struct file_download *, struct session *);
void download_manager(struct session *ses);
extern struct hierbox_browser download_browser;

/* Draws a progress bar meter or progress coloured text depending on whether
 * @text is NULL. If @meter_color is NULL dialog.meter color is used. */
void
download_progress_bar(struct terminal *term, int x, int y, int width,
		      unsigned char *text, struct color_pair *meter_color,
		      longlong current, longlong total);

#endif
