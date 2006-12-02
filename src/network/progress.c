/* Downloads progression stuff. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "network/progress.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/time.h"

#define SPD_DISP_TIME		((milliseconds_T) 100)
#define CURRENT_SPD_AFTER	((milliseconds_T) 100)


int
has_progress(struct progress *progress)
{
	timeval_T current_speed_after;

	timeval_from_milliseconds(&current_speed_after, CURRENT_SPD_AFTER);

	return (timeval_cmp(&progress->elapsed, &current_speed_after) >= 0);
}

struct progress *
init_progress(off_t start)
{
	struct progress *progress = mem_calloc(1, sizeof(*progress));

	if (progress) {
		progress->start = start;
		progress->timer = TIMER_ID_UNDEF;
	}

	return progress;
}

void
done_progress(struct progress *progress)
{
	assert(progress->timer == TIMER_ID_UNDEF);
	mem_free(progress);
}

/* Called from the timer callback of @progress->timer.  This function
 * erases the expired timer ID on behalf of the actual callback.  */
void
update_progress(struct progress *progress, off_t loaded, off_t size, off_t pos)
{
	off_t bytes_delta;
	timeval_T now, elapsed, dis_b_max, dis_b_interval;

	timeval_now(&now);
	timeval_sub(&elapsed, &progress->last_time, &now);
	timeval_copy(&progress->last_time, &now);

	progress->loaded = loaded;
	bytes_delta = progress->loaded - progress->last_loaded;
	progress->last_loaded = progress->loaded;

	timeval_add_interval(&progress->elapsed, &elapsed);

	timeval_add_interval(&progress->dis_b, &elapsed);
	timeval_from_milliseconds(&dis_b_max, mult_ms(SPD_DISP_TIME, CURRENT_SPD_SEC));
	timeval_from_milliseconds(&dis_b_interval, SPD_DISP_TIME);

	while (timeval_cmp(&progress->dis_b, &dis_b_max) >= 0) {
		progress->cur_loaded -= progress->data_in_secs[0];
		memmove(progress->data_in_secs, progress->data_in_secs + 1,
			sizeof(*progress->data_in_secs) * (CURRENT_SPD_SEC - 1));
		progress->data_in_secs[CURRENT_SPD_SEC - 1] = 0;
		timeval_sub_interval(&progress->dis_b, &dis_b_interval);
	}
	progress->data_in_secs[CURRENT_SPD_SEC - 1] += bytes_delta;
	progress->cur_loaded += bytes_delta;

	progress->current_speed = progress->cur_loaded / (CURRENT_SPD_SEC * ((long) SPD_DISP_TIME) / 1000);

	progress->pos = pos;
	progress->size = size;
	if (progress->size != -1 && progress->size < progress->pos)
		progress->size = progress->pos;

	progress->average_speed = timeval_div_off_t(progress->loaded, &progress->elapsed);
	if (progress->average_speed)	/* Division by zero risk */
		timeval_from_seconds(&progress->estimated_time,
				   (progress->size - progress->pos) / progress->average_speed);

	install_timer(&progress->timer, SPD_DISP_TIME, progress->timer_func, progress->timer_func_data);
	/* The expired timer ID has now been erased.  */
}

/* As in @install_timer, @timer_func should erase the expired timer ID
 * from @progress->timer.  The usual way to ensure this is to make
 * @timer_func call @update_progress, which sets a new timer.  */
void
start_update_progress(struct progress *progress, void (*timer_func)(void *),
		      void *timer_func_data)
{
	if (!progress->valid) {
		struct progress tmp;

		/* Just copy useful fields from invalid progress. */
		memset(&tmp, 0, sizeof(tmp));
		tmp.start = progress->start;
		tmp.seek  = progress->seek;
		tmp.valid = 1;

		memcpy(progress, &tmp, sizeof(*progress));
	}
	timeval_now(&progress->last_time);
	progress->last_loaded = progress->loaded;
	progress->timer_func = timer_func;
	progress->timer_func_data = timer_func_data;
}
