/* festival */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_FORK

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "document/document.h"
#include "document/view.h"
#include "main/select.h"
#include "osdep/osdep.h"
#include "protocol/common.h"
#include "util/box.h"
#include "util/string.h"
#include "viewer/action.h"
#include "viewer/text/draw.h"
#include "viewer/text/festival.h"
#include "viewer/text/vs.h"

struct fest festival;


static void read_from_festival(struct fest *);
static void write_to_festival(struct fest *);

static void
read_from_festival(struct fest *fest)
{
	while (1) {
		unsigned char input[512];
		int r = safe_read(fest->in, input, 512);

		if (r <= 0) break;

		if (input[r - 1] == ' ') {
			write_to_festival(fest);
			break;
		}
	}
	set_handlers(fest->in, (select_handler_T) read_from_festival,
		     NULL, NULL, fest);
}

#define FESTIVAL_SYSTEM	0
#define FLITE_SYSTEM	1

static void
write_to_festival(struct fest *fest)
{
	struct string buf;
	int i, w;
	int len;
	struct document_view *doc_view = fest->doc_view;
	struct document *doc = doc_view->document;
	struct screen_char *data;

	if (fest->line >= doc->height)
		fest->running = 0;
	if (!fest->running)
		return;

	len = doc->data[fest->line].length;
	int_upper_bound(&len, 512);

	if (!init_string(&buf))
		return;

	data = doc->data[fest->line].chars;
	if (festival.festival_or_flite == FESTIVAL_SYSTEM)
		add_to_string(&buf, "(SayText \"");
	/* UTF-8 not supported yet. Does festival support UTF-8? */
	for (i = 0; i < len; i++) {
		unsigned char ch = (unsigned char)data[i].data;

		if (ch == '"' || ch == '\\' || ch == '\'')
			add_char_to_string(&buf, '\\');
		add_char_to_string(&buf, ch);
	}
	if (festival.festival_or_flite == FESTIVAL_SYSTEM)
		add_to_string(&buf, "\")");
	add_char_to_string(&buf, '\n');

	w = safe_write(fest->out, buf.source, buf.length);
	if (w >= 0) {
		if (fest->line >= doc_view->vs->y + doc_view->box.height) {
			move_page_down(doc_view->session, doc_view);
			refresh_view(doc_view->session, doc_view, 0);
		}
		fest->line++;
	}
	done_string(&buf);
}

static int
init_festival(void)
{
	int in_pipe[2] = {-1, -1};
	int out_pipe[2] = {-1, -1};
	pid_t cpid;

	festival.festival_or_flite = get_opt_int("document.speech.system");
	if (festival.festival_or_flite == FESTIVAL_SYSTEM) {
		if (access(FESTIVAL, X_OK)) return 1;
	} else {
		if (access(FLITE, X_OK)) return 1;
	}

	if (c_pipe(in_pipe) || c_pipe(out_pipe)) {
		if (in_pipe[0] >= 0) close(in_pipe[0]);
		if (in_pipe[1] >= 0) close(in_pipe[1]);
		if (out_pipe[0] >= 0) close(out_pipe[0]);
		if (out_pipe[1] >= 0) close(out_pipe[1]);
		return 1;
	}

	cpid = fork();
	if (cpid == -1) {
		close(in_pipe[0]);
		close(in_pipe[1]);
		close(out_pipe[0]);
		close(out_pipe[1]);
		return 1;
	}
	if (!cpid) {
		dup2(out_pipe[1], 1);
		dup2(in_pipe[0], 0);
		close(out_pipe[0]);
		close(in_pipe[1]);
		close(2);
		close_all_non_term_fd();
		if (festival.festival_or_flite == FESTIVAL_SYSTEM) {
			execl(FESTIVAL, "festival", "-i", NULL);
			_exit(0);
		} else {
			char line[1024];
			char command[1200];

			do {
				fgets(line, 1024, stdin);
				snprintf(command, 1200, "%s -t '%s'", FLITE, line);
				system(command);
				putchar(' ');
				fflush(stdout);
			} while (!feof(stdin));
			_exit(0);
		}
	} else {
		close(out_pipe[1]);
		close(in_pipe[0]);
		festival.in = out_pipe[0];
		festival.out = in_pipe[1];
		set_nonblocking_fd(festival.out);
		set_handlers(festival.in, (select_handler_T) read_from_festival,
		     NULL, NULL, &festival);
		return 0;
	}
}
#undef FESTIVAL_SYSTEM
#undef FLITE_SYSTEM

void
run_festival(struct session *ses, struct document_view *doc_view)
{
	if (festival.in == -1 && init_festival())
		return;

	festival.doc_view = doc_view;
	festival.line = doc_view->vs->y;
	festival.running = !festival.running;

	if (festival.running) {
		write_to_festival(&festival);
	}
}
#endif
