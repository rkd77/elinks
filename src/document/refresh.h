/* $Id: refresh.h,v 1.6 2005/06/13 00:43:27 jonas Exp $ */

#ifndef EL__DOCUMENT_REFRESH_H
#define EL__DOCUMENT_REFRESH_H

#include "main/timer.h" /* timer_id_T */

struct session;
struct uri;

struct document_refresh {
	timer_id_T timer;
	unsigned long seconds;
	struct uri *uri;
	unsigned int restart:1;
};

struct document_refresh *init_document_refresh(unsigned char *url, unsigned long seconds);
void done_document_refresh(struct document_refresh *refresh);
void kill_document_refresh(struct document_refresh *refresh);
void start_document_refresh(struct document_refresh *refresh, struct session *ses);

#endif
