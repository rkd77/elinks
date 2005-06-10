#ifndef EL__VIEWER_ACTION_H
#define EL__VIEWER_ACTION_H

#include "config/kbdbind.h"

struct session;

enum frame_event_status {
	/* The event was not handled */
	FRAME_EVENT_IGNORED,
	/* The event was handled, and the screen should be redrawn */
	FRAME_EVENT_REFRESH,
	/* The event was handled, and the screen should _not_ be redrawn */
	FRAME_EVENT_OK,
	/* The event was handled, and the current session was destroyed */
	FRAME_EVENT_SESSION_DESTROYED,
};

enum frame_event_status do_action(struct session *ses,
                                  enum main_action action_id, int verbose);

#endif
