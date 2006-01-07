/* Terminal interface - low-level displaying implementation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bookmarks/bookmarks.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/main.h"
#include "main/object.h"
#include "main/select.h"
#include "osdep/osdep.h"
#include "osdep/signals.h"
#include "session/session.h"
#include "terminal/draw.h"
#include "terminal/event.h"
#include "terminal/hardio.h"
#include "terminal/kbd.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/textarea.h"


INIT_LIST_HEAD(terminals);


static void check_if_no_terminal(void);


void
redraw_terminal(struct terminal *term)
{
	struct term_event ev;

	set_redraw_term_event(&ev, term->width, term->height);
	term_send_event(term, &ev);
}

void
redraw_terminal_cls(struct terminal *term)
{
	struct term_event ev;

	set_resize_term_event(&ev, term->width, term->height);
	term_send_event(term, &ev);
}

void
cls_redraw_all_terminals(void)
{
	struct terminal *term;

	foreach (term, terminals)
		redraw_terminal_cls(term);
}

struct terminal *
init_term(int fdin, int fdout)
{
	struct terminal *term = mem_calloc(1, sizeof(*term));

	if (!term) {
		check_if_no_terminal();
		return NULL;
	}

	term->screen = init_screen();
	if (!term->screen) {
		mem_free(term);
		return NULL;
	}

	init_list(term->windows);

	term->fdin = fdin;
	term->fdout = fdout;
	term->master = (term->fdout == get_output_handle());
	term->blocked = -1;
	term->spec = get_opt_rec(config_options, "terminal._template_");
	object_lock(term->spec);

	add_to_list(terminals, term);

	set_handlers(fdin, (select_handler_T) in_term, NULL,
		     (select_handler_T) destroy_terminal, term);
	return term;
}

void
redraw_all_terminals(void)
{
	struct terminal *term;

	foreach (term, terminals)
		redraw_screen(term);
}

void
destroy_terminal(struct terminal *term)
{
#ifdef CONFIG_BOOKMARKS
	bookmark_auto_save_tabs(term);
#endif

	while (!list_empty(term->windows))
		delete_window(term->windows.next);

	/* mem_free_if(term->cwd); */
	mem_free_if(term->title);
	if (term->screen) done_screen(term->screen);

	clear_handlers(term->fdin);
	mem_free_if(term->interlink);

	if (term->blocked != -1) {
		close(term->blocked);
		clear_handlers(term->blocked);
	}

	del_from_list(term);
	close(term->fdin);

	if (term->fdout != 1) {
		if (term->fdout != term->fdin) close(term->fdout);
	} else {
		unhandle_terminal_signals(term);
		free_all_itrms();
#ifndef NO_FORK_ON_EXIT
		if (!list_empty(terminals)) {
			if (fork()) exit(0);
		}
#endif
	}

	object_unlock(term->spec);
	mem_free(term);
	check_if_no_terminal();
}

void
destroy_all_terminals(void)
{
	while (!list_empty(terminals))
		destroy_terminal(terminals.next);
}

static void
check_if_no_terminal(void)
{
	program.terminate = list_empty(terminals)
			    && !get_opt_bool("ui.sessions.keep_session_active");
}

void
exec_thread(unsigned char *path, int p)
{
	int plen = strlen(path + 1) + 2;

#if defined(HAVE_SETPGID) && !defined(CONFIG_BEOS) && !defined(HAVE_BEGINTHREAD)
	if (path[0] == 2) setpgid(0, 0);
#endif
	exe(path + 1);
	if (path[plen]) unlink(path + plen);
}

void
close_handle(void *h)
{
	close((long) h);
	clear_handlers((long) h);
}

static void
unblock_terminal(struct terminal *term)
{
	close_handle((void *) (long) term->blocked);
	term->blocked = -1;
	set_handlers(term->fdin, (select_handler_T) in_term, NULL,
		     (select_handler_T) destroy_terminal, term);
	unblock_itrm(term->fdin);
	redraw_terminal_cls(term);
	if (textarea_editor)	/* XXX */
		textarea_edit(1, NULL, NULL, NULL, NULL);
}


static void
exec_on_master_terminal(struct terminal *term,
			unsigned char *path, int plen,
		 	unsigned char *delete, int dlen,
			int fg)
{
	int blockh;
	unsigned char *param;
	int param_size;

	if (is_blocked() && fg) {
		unlink(delete);
		return;
	}

	param_size = plen + dlen + 2 /* 2 null char */ + 1 /* fg */;
	param = mem_alloc(param_size);
	if (!param) return;

	param[0] = fg;
	memcpy(param + 1, path, plen + 1);
	memcpy(param + 1 + plen + 1, delete, dlen + 1);

	if (fg == 1) block_itrm(term->fdin);

	blockh = start_thread((void (*)(void *, int)) exec_thread,
			      param, param_size);
	if (blockh == -1) {
		if (fg == 1) unblock_itrm(term->fdin);
		mem_free(param);
		return;
	}

	mem_free(param);
	if (fg == 1) {
		term->blocked = blockh;
		set_handlers(blockh,
			     (select_handler_T) unblock_terminal,
			     NULL,
			     (select_handler_T) unblock_terminal,
			     term);
		set_handlers(term->fdin, NULL, NULL,
			     (select_handler_T) destroy_terminal,
			     term);
		/* block_itrm(term->fdin); */
	} else {
		set_handlers(blockh, close_handle, NULL,
			     close_handle, (void *) (long) blockh);
	}
}

static void
exec_on_slave_terminal( struct terminal *term,
		 	unsigned char *path, int plen,
		 	unsigned char *delete, int dlen,
			int fg)
{
	int data_size = plen + dlen + 1 /* 0 */ + 1 /* fg */ + 2 /* 2 null char */;
	unsigned char *data = fmem_alloc(data_size);

	if (!data) return;
	
	data[0] = 0;
	data[1] = fg;
	memcpy(data + 2, path, plen + 1);
	memcpy(data + 2 + plen + 1, delete, dlen + 1);
	hard_write(term->fdout, data, data_size);
	fmem_free(data);
}

void
exec_on_terminal(struct terminal *term, unsigned char *path,
		 unsigned char *delete, int fg)
{
	if (path) {
		if (!*path) return;
	} else {
		path = "";
	}

#ifdef NO_FG_EXEC
	fg = 0;
#endif

	if (term->master) {
		if (!*path) {
			dispatch_special(delete);
			return;
		}

		exec_on_master_terminal(term,
					path, strlen(path),
		 			delete, strlen(delete),
					fg);
	} else {
		exec_on_slave_terminal( term,
					path, strlen(path),
		 			delete, strlen(delete),
					fg);
	}
}

void
exec_shell(struct terminal *term)
{
	unsigned char *sh;

	if (!can_open_os_shell(term->environment)) return;

	sh = get_shell();
	if (sh && *sh)
		exec_on_terminal(term, sh, "", 1);
}


void
do_terminal_function(struct terminal *term, unsigned char code,
		     unsigned char *data)
{
	int data_len = strlen(data);
	unsigned char *x_data = fmem_alloc(data_len + 1 /* code */ + 1 /* null char */);

	if (!x_data) return;
	x_data[0] = code;
	memcpy(x_data + 1, data, data_len + 1);
	exec_on_terminal(term, NULL, x_data, 0);
	fmem_free(x_data);
}

void
set_terminal_title(struct terminal *term, unsigned char *title)
{
	if (term->title && !strcmp(title, term->title)) return;
	mem_free_set(&term->title, stracpy(title));
	do_terminal_function(term, TERM_FN_TITLE, title);
}

static int terminal_pipe[2];

int
check_terminal_pipes(void)
{
	return c_pipe(terminal_pipe);
}

void
close_terminal_pipes(void)
{
	close(terminal_pipe[0]);
	close(terminal_pipe[1]);
}

struct terminal *
attach_terminal(int in, int out, int ctl, void *info, int len)
{
	struct terminal *term;

	if (set_nonblocking_fd(terminal_pipe[0]) < 0) return NULL;
	if (set_nonblocking_fd(terminal_pipe[1]) < 0) return NULL;
	handle_trm(in, out, out, terminal_pipe[1], ctl, info, len, 0);

	term = init_term(terminal_pipe[0], out);
	if (!term) {
		close_terminal_pipes();
		return NULL;
	}

	return term;
}
