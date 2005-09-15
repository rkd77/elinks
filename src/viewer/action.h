/* $Id: action.h,v 1.7 2004/10/10 00:43:33 miciah Exp $ */

#ifndef EL__SCHED_ACTION_H
#define EL__SCHED_ACTION_H

#include "config/kbdbind.h"
#include "sched/session.h"
#include "viewer/text/view.h"

enum frame_event_status do_action(struct session *ses, enum main_action action,
				  int verbose);

#endif
