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
#include <stdio.h>

#include "elinks.h"

#include "document/document.h"
#include "document/view.h"
#include "intl/charsets.h"
#include "main/select.h"
#include "osdep/osdep.h"
#include "protocol/common.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
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

/* These numbers must match the documentation of the
 * "document.speech.system" option.  */
#define FESTIVAL_SYSTEM	0
#define FLITE_SYSTEM	1
#define ESPEAK_SYSTEM	2

static void
write_to_festival(struct fest *fest)
{
	struct string buf;
	int i, w, len;
	struct document_view *doc_view = fest->doc_view;
	struct document *doc = NULL;
	struct screen_char *data;

	if (!doc_view) {
		fest->running = 0;
	} else {
		doc = doc_view->document;
		assert(doc);
		if (fest->line >= doc->height)
			fest->running = 0;
	}

	if (!fest->running)
		return;

	len = doc->data[fest->line].length;

	if (!init_string(&buf))
		return;

	data = doc->data[fest->line].chars;

	/* The terminal codepage is used for the speech output. */
#ifdef CONFIG_UTF8
	if (!doc_view->session->tab->term->utf8)
#endif
	if (fest->speech_system == FESTIVAL_SYSTEM) {
		/* The string generated here will be at most
		 * 10+MAX_LINE_LENGTH*2+2+1 bytes long.  */ 
		add_to_string(&buf, "(SayText \"");

		for (i = 0; i < len; i++) {
			unsigned char ch = (unsigned char)data[i].data;
			unsigned char attr = data[i].attr;
			int old_len = buf.length;

			if (attr & SCREEN_ATTR_FRAME) continue;

			if (ch == '"' || ch == '\\')
				add_char_to_string(&buf, '\\');
			add_char_to_string(&buf, ch);
			if (buf.length > PIPE_BUF - 3) {
				buf.length = old_len;
				break;
			}
		}
		add_to_string(&buf, "\")");
	} else { /* flite */
		int_upper_bound(&len, PIPE_BUF - 1);
		for (i = 0; i < len; i++) {
			unsigned char ch = (unsigned char)data[i].data;
			unsigned char attr = data[i].attr;

			if (attr & SCREEN_ATTR_FRAME) continue;
			add_char_to_string(&buf, ch);
		}
	}
#ifdef CONFIG_UTF8
	else {
		if (fest->speech_system == FESTIVAL_SYSTEM) {
			add_to_string(&buf, "(SayText \"");
			for (i = 0; i < len; i++) {
				unsigned char *text;
				unsigned char attr = data[i].attr;
				int old_len = buf.length;

				if (attr & SCREEN_ATTR_FRAME) continue;

				text = encode_utf8(data[i].data);
				if (text[0] == '"' || text[0] == '\\')
					add_char_to_string(&buf, '\\');
				add_to_string(&buf, text);
				if (buf.length > PIPE_BUF - 3) {
					buf.length = old_len;
					break;
				}
			}
			add_to_string(&buf, "\")");
		} else {
			for (i = 0; i < len; i++) {
				unsigned char *text;
				unsigned char attr = data[i].attr;
				int old_len = buf.length;

				if (attr & SCREEN_ATTR_FRAME) continue;

				text = encode_utf8(data[i].data);
				add_to_string(&buf, text);
				if (buf.length > PIPE_BUF - 1) {
					buf.length = old_len;
					break;
				}
			}
		}
	}
#endif
	add_char_to_string(&buf, '\n');
#if 0
	/* Cannot allow a short write here.  It would probably cause
	 * an unpaired quote character to be sent to festival,
	 * enabling a command injection attack.  If buf.length <=
	 * PIPE_BUF, then the write will occur either completely or
	 * not at all.  Compare to _POSIX_PIPE_BUF instead, to ensure
	 * portability to systems with an unusually low PIPE_BUF.
	 * If this assertion fails, that means MAX_LINE_LENGTH was
	 * set too high.  */
	assert(buf.length <= _POSIX_PIPE_BUF);
	if_assert_failed { buf.length = 0; }
#endif

	w = safe_write(fest->out, buf.source, buf.length);
	if (w >= 0) {
		assert(w == buf.length);
		if_assert_failed {
			/* The input parser of the festival process is
			 * now in an unknown state.  */
			clear_handlers(fest->in);
			close(fest->in);
			fest->in = -1;
			clear_handlers(fest->out);
			close(fest->out);
			fest->out = -1;
		}

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

	festival.speech_system = get_opt_int("document.speech.system");
	switch (festival.speech_system) {
	case FESTIVAL_SYSTEM:
	default:
		if (access(FESTIVAL, X_OK)) return 1;
		break;
	case FLITE_SYSTEM:
		if (access(FLITE, X_OK)) return 1;
		break;
	case ESPEAK_SYSTEM:
		if (access(ESPEAK, X_OK)) return 1;
		break;
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
		unsigned char *program;

		dup2(out_pipe[1], 1);
		dup2(in_pipe[0], 0);
		close(out_pipe[0]);
		close(in_pipe[1]);
		close(2);
		close_all_non_term_fd();
		switch (festival.speech_system) {
		case FESTIVAL_SYSTEM:
		default:
			execl(FESTIVAL, "festival", "-i", NULL);
			_exit(0);
		case FLITE_SYSTEM:
			program = FLITE;
			break;
		case ESPEAK_SYSTEM:
			program = ESPEAK;
			break;
		}
		do {
			char line[2048];
			FILE *out;

			fgets(line, 2048, stdin);
			out = popen(program, "w");
			if (out) {
				fputs(line, out);
				pclose(out);
				putchar(' ');
				fflush(stdout);
			}
		} while (!feof(stdin));
		_exit(0);
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
#undef ESPEAK_SYSTEM

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

void
stop_festival(struct document_view *doc_view)
{
	if (festival.doc_view == doc_view)
		festival.doc_view = NULL;
}

#endif
