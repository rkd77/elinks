/* $Id: location.h,v 1.17 2005/06/14 12:25:21 jonas Exp $ */

#ifndef EL__SESSION_LOCATION_H
#define EL__SESSION_LOCATION_H

#include "session/download.h"
#include "util/lists.h"
#include "viewer/text/vs.h"

struct location {
	LIST_HEAD(struct location);

	struct list_head frames;
	struct download download;
	struct view_state vs;
};


void copy_location(struct location *, struct location *);

/* You probably want to call del_from_history() first! */
void destroy_location(struct location *);

#endif
