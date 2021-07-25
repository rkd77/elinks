#ifndef EL__SESSION_LOCATION_H
#define EL__SESSION_LOCATION_H

#include "session/download.h"
#include "session/session.h"
#include "util/lists.h"
#include "viewer/text/vs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct location {
	LIST_HEAD(struct location);

	LIST_OF(struct frame) frames;
	LIST_OF(struct frame) iframes;
	struct download download;
	struct view_state vs;
};


void copy_location(struct location *, struct location *);

/** You probably want to call del_from_history() first!
 * @relates location */
void destroy_location(struct location *);

#ifdef __cplusplus
}
#endif

#endif
