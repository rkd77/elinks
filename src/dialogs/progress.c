/* Display of downloads progression stuff. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "intl/gettext/libintl.h"
#include "network/progress.h"
#include "terminal/draw.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


unsigned char *
get_progress_msg(struct progress *progress, struct terminal *term,
		 int wide, int full, unsigned char *separator)
{
	struct string msg;
	int newlines = separator[strlen(separator) - 1] == '\n';

	if (!init_string(&msg)) return NULL;

	/* FIXME: The following is a PITA from the l10n standpoint. A *big*
	 * one, _("of")-like pearls are a nightmare. Format strings need to
	 * be introduced to this fuggy corner of code as well. --pasky */

	add_to_string(&msg, _("Received", term));
	add_char_to_string(&msg, ' ');
	add_xnum_to_string(&msg, progress->pos);
	if (progress->size >= 0) {
		add_char_to_string(&msg, ' ');
		add_to_string(&msg, _("of", term));
		add_char_to_string(&msg, ' ');
		add_xnum_to_string(&msg, progress->size);
	}

	add_to_string(&msg, separator);

	if (wide) {
		/* Do the following only if there is room */

		add_to_string(&msg,
			      _(full ? (newlines ? N_("Average speed")
					         : N_("average speed"))
				     : N_("avg"),
				term));
		add_char_to_string(&msg, ' ');
		add_xnum_to_string(&msg, progress->average_speed);
		add_to_string(&msg, "/s");

		add_to_string(&msg, ", ");
		add_to_string(&msg,
			      _(full ? N_("current speed") : N_("cur"), term));
		add_char_to_string(&msg, ' '),
		add_xnum_to_string(&msg, progress->current_speed);
		add_to_string(&msg, "/s");

		add_to_string(&msg, separator);

		add_to_string(&msg, _(full ? (newlines ? N_("Elapsed time")
						       : N_("elapsed time"))
					   : N_("ETT"),
				   term));
		add_char_to_string(&msg, ' ');
		add_timeval_to_string(&msg, &progress->elapsed);

	} else {
		add_to_string(&msg, _(newlines ? N_("Speed") : N_("speed"),
					term));

		add_char_to_string(&msg, ' ');
		add_xnum_to_string(&msg, progress->average_speed);
		add_to_string(&msg, "/s");
	}

	if (progress->size >= 0 && progress->loaded > 0) {
		add_to_string(&msg, ", ");
		add_to_string(&msg, _(full ? N_("estimated time")
					   : N_("ETA"),
				      term));
		add_char_to_string(&msg, ' ');
		add_timeval_to_string(&msg, &progress->estimated_time);
	}

	return msg.source;
}

void
draw_progress_bar(struct progress *progress, struct terminal *term,
		  int x, int y, int width,
		  unsigned char *text, struct color_pair *meter_color)
{
	/* Note : values > 100% are theorically possible and were seen. */
	int percent = 0;
	struct box barprogress;

	if (progress->size > 0)
		percent = (int) ((longlong) 100 * progress->pos / progress->size);

	/* Draw the progress meter part "[###    ]" */
	if (!text && width > 2) {
		width -= 2;
		draw_text(term, x++, y, "[", 1, 0, NULL);
		draw_text(term, x + width, y, "]", 1, 0, NULL);
	}

	if (!meter_color) meter_color = get_bfu_color(term, "dialog.meter");
	set_box(&barprogress,
		x, y, int_min(width * percent / 100, width), 1);
	draw_box(term, &barprogress, ' ', 0, meter_color);

	/* On error, will print '?' only, should not occur. */
	if (text) {
		width = int_min(width, strlen(text));

	} else if (width > 1) {
		static unsigned char s[] = "????"; /* Reduce or enlarge at will. */
		unsigned int slen = 0;
		int max = int_min(sizeof(s), width) - 1;

		if (ulongcat(s, &slen, percent, max, 0)) {
			s[0] = '?';
			slen = 1;
		}

		s[slen++] = '%';

		/* Draw the percentage centered in the progress meter */
		x += (1 + width - slen) / 2;

		assert(slen <= width);
		width = slen;
		text = s;
	}

	draw_text(term, x, y, text, width, 0, NULL);
}
