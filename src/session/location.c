/** Locations handling
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "session/location.h"
#include "session/session.h"
#include "util/memory.h"
#include "util/string.h"


/** @relates location */
void
copy_location(struct location *dst, struct location *src)
{
	struct frame *frame, *new_frame;

	init_list(dst->frames);
	foreachback (frame, src->frames) {
		new_frame = mem_calloc(1, sizeof(*new_frame));
		if (new_frame) {
			new_frame->name = stracpy(frame->name);
			if (!new_frame->name) {
				mem_free(new_frame);
				return;
			}
			new_frame->redirect_cnt = 0;
			copy_vs(&new_frame->vs, &frame->vs);
			add_to_list(dst->frames, new_frame);
		}
	}
	copy_vs(&dst->vs, &src->vs);
}

void
destroy_location(struct location *loc)
{
	struct frame *frame;

	foreach (frame, loc->frames) {
		destroy_vs(&frame->vs, 1);
		mem_free(frame->name);
	}
	free_list(loc->frames);
	destroy_vs(&loc->vs, 1);
	mem_free(loc);
}
