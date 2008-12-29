/** Terminal interface - low-level displaying implementation.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bookmarks/bookmarks.h"
#include "config/options.h"
#include "main/main.h"
#include "main/module.h"
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


INIT_LIST_OF(struct terminal, terminals);

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
	unsigned char name[MAX_TERM_LEN + 9] = "terminal.";
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

	get_terminal_name(name + 9);
	term->spec = get_opt_rec(config_options, name);
	object_lock(term->spec);

	add_to_list(terminals, term);

	set_handlers(fdin, (select_handler_T) in_term, NULL,
		     (select_handler_T) destroy_terminal, term);
	return term;
}

/** Get the codepage of a terminal.  The UTF-8 I/O option does not
 * affect this.
 *
 * @todo Perhaps cache the value in struct terminal?
 *
 * @bug Bug 1064: If the charset has been set as "System", this should
 * apply the locale environment variables of the slave ELinks process,
 * not those of the master ELinks process that parsed the configuration
 * file.  That is why the parameter points to struct terminal and not
 * merely to its option tree (term->spec).
 *
 * @see get_translation_table(), get_cp_mime_name() */
int
get_terminal_codepage(const struct terminal *term)
{
	return get_opt_codepage_tree(term->spec, "charset");
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
	detach_downloads_from_terminal(term);

	/* delete_window doesn't update term->current_tab, but it
	   calls redraw_terminal, which requires term->current_tab
	   to be valid if there are any tabs left.  So set a value
	   that will be valid for that long.  */
	term->current_tab = 0;

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

#if defined(HAVE_SETPGID) && !defined(CONFIG_OS_BEOS) && !defined(HAVE_BEGINTHREAD)
	if (path[0] == TERM_EXEC_NEWWIN) setpgid(0, 0);
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
	unblock_itrm();
	redraw_terminal_cls(term);
	if (textarea_editor)	/* XXX */
		textarea_edit(1, NULL, NULL, NULL, NULL);
}

#ifndef CONFIG_FASTMEM
void
assert_terminal_ptr_not_dangling(const struct terminal *suspect)
{
	struct terminal *term;

	if (suspect == NULL)
		return;

	foreach (term, terminals) {
		if (term == suspect)
			return;
	}

	assertm(0, "Dangling pointer to struct terminal");
}
#endif /* !CONFIG_FASTMEM */

static void
exec_on_master_terminal(struct terminal *term,
			unsigned char *path, int plen,
		 	unsigned char *delete, int dlen,
			enum term_exec fg)
{
	int blockh;
	int param_size = plen + dlen + 2 /* 2 null char */ + 1 /* fg */;
	unsigned char *param = fmem_alloc(param_size);

	if (!param) return;

	param[0] = fg;
	memcpy(param + 1, path, plen + 1);
	memcpy(param + 1 + plen + 1, delete, dlen + 1);

	if (fg == TERM_EXEC_FG) block_itrm();

	blockh = start_thread((void (*)(void *, int)) exec_thread,
			      param, param_size);
	fmem_free(param);
	if (blockh == -1) {
		if (fg == TERM_EXEC_FG) unblock_itrm();
		return;
	}

	if (fg == TERM_EXEC_FG) {
		term->blocked = blockh;
		set_handlers(blockh,
			     (select_handler_T) unblock_terminal,
			     NULL,
			     (select_handler_T) unblock_terminal,
			     term);
		set_handlers(term->fdin, NULL, NULL,
			     (select_handler_T) destroy_terminal,
			     term);

	} else {
		set_handlers(blockh, close_handle, NULL,
			     close_handle, (void *) (long) blockh);
	}
}

static void
exec_on_slave_terminal( struct terminal *term,
		 	unsigned char *path, int plen,
		 	unsigned char *delete, int dlen,
			enum term_exec fg)
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
		 unsigned char *delete, enum term_exec fg)
{
	if (path) {
		if (!*path) return;
	} else {
		path = "";
	}

#ifdef NO_FG_EXEC
	fg = TERM_EXEC_BG;
#endif

	if (term->master) {
		if (!*path) {
			dispatch_special(delete);
			return;
		}

		/* TODO: Should this be changed to allow TERM_EXEC_NEWWIN
		 * in a blocked terminal?  There is similar code in
		 * in_sock().  --KON, 2007 */
		if (fg != TERM_EXEC_BG && is_blocked()) {
			unlink(delete);
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
		exec_on_terminal(term, sh, "", TERM_EXEC_FG);
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
	exec_on_terminal(term, NULL, x_data, TERM_EXEC_BG);
	fmem_free(x_data);
}

/** @return negative on error; zero or positive on success.  */
int
set_terminal_title(struct terminal *term, unsigned char *title)
{
	int from_cp;
	int to_cp;
	unsigned char *converted = NULL;

	if (term->title && !strcmp(title, term->title)) return 0;

	/* In which codepage was the title parameter given?  */
	from_cp = get_terminal_codepage(term);

	/* In which codepage does the terminal want the title?  */
	if (get_opt_bool_tree(term->spec, "latin1_title"))
		to_cp = get_cp_index("ISO-8859-1");
	else if (get_opt_bool_tree(term->spec, "utf_8_io"))
		to_cp = get_cp_index("UTF-8");
	else
		to_cp = from_cp;

	if (from_cp != to_cp) {
		struct conv_table *convert_table;

		convert_table = get_translation_table(from_cp, to_cp);
		if (!convert_table) return -1;
		converted = convert_string(convert_table, title, strlen(title),
					   to_cp, CSM_NONE, NULL, NULL, NULL);
		if (!converted) return -1;
	}

	mem_free_set(&term->title, stracpy(title));
	do_terminal_function(term, TERM_FN_TITLE_CODEPAGE,
			     get_cp_mime_name(to_cp));
	do_terminal_function(term, TERM_FN_TITLE,
			     converted ? converted : title);
	mem_free_if(converted);
	return 0;
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

static struct module *terminal_submodules[] = {
	&terminal_screen_module,
	NULL
};

struct module terminal_module = struct_module(
	/* Because this module is listed in main_modules rather than
	 * in builtin_modules, its name does not appear in the user
	 * interface and so need not be translatable.  */
	/* name: */		"Terminal",
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	terminal_submodules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
