/* Lua interface (scripting engine) */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <setjmp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <lua.h>
#include <lualib.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "config/home.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "document/document.h"
#include "document/renderer.h"
#include "document/view.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "main/event.h"
#include "main/module.h"
#include "osdep/osdep.h"
#include "osdep/signals.h"
#include "protocol/uri.h"
#include "scripting/lua/core.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/dump/dump.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

#define LUA_HOOKS_FILENAME		"hooks.lua"


lua_State *lua_state;

static struct session *lua_ses;
static struct terminal *errterm;
static sigjmp_buf errjmp;

#define L	lua_state
#define LS	lua_State *S

static void handle_standard_lua_returns(unsigned char *from);
static void handle_ref(LS, struct session *ses, int func_ref,
                       unsigned char *from, int num_args, int unref);


/*
 * Functions exported to the lua_State.
 */

static int
l_alert(LS)
{
	unsigned char *msg = (unsigned char *) lua_tostring(S, 1);

	/* Don't crash if a script calls e.g. error(nil) or error(error).  */
	if (msg == NULL)
		msg = "(cannot convert the error message to a string)";

	alert_lua_error(msg);
	return 0;
}

static int
l_current_url(LS)
{
	if (lua_ses && have_location(lua_ses)) {
		struct view_state *vs = &cur_loc(lua_ses)->vs;
		unsigned char *url = get_uri_string(vs->uri, URI_ORIGINAL);

		if (url) {
			lua_pushstring(S, url);
			mem_free(url);
			return 1;
		}
	}

	lua_pushnil(S);
	return 1;
}

static int
l_current_link(LS)
{
	struct link *link = get_current_session_link(lua_ses);

	if (link) {
		lua_pushstring(S, link->where);
	} else {
		lua_pushnil(S);
	}

	return 1;
}

static int
l_current_title(LS)
{
	struct document_view *doc_view = current_frame(lua_ses);

	if (doc_view && doc_view->document->title) {
		unsigned char *clean_title = stracpy(doc_view->document->title);

		if (clean_title) {
			sanitize_title(clean_title);

			lua_pushstring(S, clean_title);
			mem_free(clean_title);
			return 1;
		}
	}

	lua_pushnil(S);
	return 1;
}

static int
l_current_document(LS)
{
	if (lua_ses && lua_ses->doc_view && lua_ses->doc_view->document) {
		struct cache_entry *cached = lua_ses->doc_view->document->cached;
		struct fragment *f = cached ? cached->frag.next : NULL;

		if (f && f->length) {
			lua_pushlstring(S, f->data, f->length);
			return 1;
		}
	}

	lua_pushnil(S);
	return 1;
}

/* XXX: This function is mostly copied from `dump_to_file'. */
static int
l_current_document_formatted(LS)
{
	struct document_view *doc_view;
	struct string buffer;
	int width, old_width = 0;

	if (lua_gettop(S) == 0) width = -1;
	else if (!lua_isnumber(S, 1)) goto lua_error;
	else if ((width = lua_tonumber(S, 1)) <= 0) goto lua_error;

	if (!lua_ses || !(doc_view = current_frame(lua_ses))) goto lua_error;
	if (width > 0) {
		old_width = lua_ses->tab->term->width;
		lua_ses->tab->term->width = width;
		render_document_frames(lua_ses, 0);
	}

	if (init_string(&buffer)) {
		add_document_to_string(&buffer, doc_view->document);
		lua_pushlstring(S, buffer.source, buffer.length);
		done_string(&buffer);
	}

	if (width > 0) {
		lua_ses->tab->term->width = old_width;
		render_document_frames(lua_ses, 0);
	}
	return 1;

lua_error:
	lua_pushnil(S);
	return 1;
}

static int
l_pipe_read(LS)
{
	FILE *fp;
	unsigned char *s = NULL;
	size_t len = 0;

	if (!lua_isstring(S, 1)) goto lua_error;

	fp = popen(lua_tostring(S, 1), "r");
	if (!fp) goto lua_error;

	while (!feof(fp)) {
		unsigned char buf[1024];
		size_t l = fread(buf, 1, sizeof(buf), fp);

		if (l > 0) {
			unsigned char *news = mem_realloc(s, len + l);

			if (!news) goto lua_error;
			s = news;
			memcpy(s + len, buf, l);
			len += l;

		} else if (l < 0) {
			goto lua_error;
		}
	}
	pclose(fp);

	lua_pushlstring(S, s, len);
	mem_free_if(s);
	return 1;

lua_error:
	mem_free_if(s);
	lua_pushnil(S);
	return 1;
}

static int
l_execute(LS)
{
	if (lua_isstring(S, 1)) {
		exec_on_terminal(lua_ses->tab->term, (unsigned char *) lua_tostring(S, 1), "",
				 TERM_EXEC_BG);
		lua_pushnumber(S, 0);
		return 1;
	}

	lua_pushnil(L);
	return 1;
}

static int
l_tmpname(LS)
{
	unsigned char *fn = tempnam(NULL, "elinks");

	if (fn) {
		lua_pushstring(S, fn);
		free(fn);
		return 1;
	}

	alert_lua_error("Error generating temporary file name");
	lua_pushnil(S);
	return 1;
}

/*
 * Helper to run Lua functions bound to keystrokes.
 */

static enum evhook_status
run_lua_func(va_list ap, void *data)
{
	struct session *ses = va_arg(ap, struct session *);
	int func_ref = (long) data;

	if (func_ref == LUA_NOREF) {
		alert_lua_error("key bound to nothing (internal error)");
		return EVENT_HOOK_STATUS_NEXT;
	}

	handle_ref(L, ses, func_ref, "keyboard function", 0, 0);

	return EVENT_HOOK_STATUS_NEXT;
}

/* bind_key (keymap, keystroke, function) */
static int
l_bind_key(LS)
{
	int ref;
	int event_id;
	unsigned char *err = NULL;
	struct string event_name = NULL_STRING;

	if (!lua_isstring(S, 1) || !lua_isstring(S, 2)
	    || !lua_isfunction(S, 3)) {
		alert_lua_error("bad arguments to bind_key");
		goto lua_error;
	}

	if (!init_string(&event_name)) goto lua_error;

	/* ELinks will need to call the Lua function when the user
	 * presses the key.  However, ELinks cannot store a pointer
	 * to the function, because the C API of Lua does not provide
	 * one.  Instead, ask the "reference system" of Lua to
	 * generate an integer key that ELinks can store.
	 *
	 * TODO: If l_bind_key() succeeds, then the function will
	 * never be removed from the reference system again, because
	 * the rest of ELinks does not tell this module if the
	 * keybinding is removed.  This is part of bug 810.  */
	lua_pushvalue(S, 3);
	ref = luaL_ref(S, LUA_REGISTRYINDEX);
	add_format_to_string(&event_name, "lua-run-func %i", ref);

	event_id = bind_key_to_event_name((unsigned char *) lua_tostring(S, 1),
					  (const unsigned char *) lua_tostring(S, 2),
					  event_name.source, &err);
	done_string(&event_name);

	if (!err) {
		event_id = register_event_hook(event_id, run_lua_func, 0,
					       (void *) (long) ref);

		if (event_id == EVENT_NONE)
			err = gettext("Error registering event hook");
	}

	if (err) {
		luaL_unref(S, LUA_REGISTRYINDEX, ref);
		alert_lua_error2("error in bind_key: ", err);
		goto lua_error;
	}

	lua_pushnumber(S, 1);
	return 1;

lua_error:
	lua_pushnil(S);
	return 1;
}


/* Begin very hackish bit for bookmark editing dialog.  */
/* XXX: Add history and generalise.  */

struct lua_dlg_data {
	lua_State *state;
	unsigned char cat[MAX_STR_LEN];
	unsigned char name[MAX_STR_LEN];
	unsigned char url[MAX_STR_LEN];
	int func_ref;
};

static void
dialog_run_lua(void *data_)
{
	struct lua_dlg_data *data = data_;
	lua_State *s = data->state;

	lua_pushstring(s, data->cat);
	lua_pushstring(s, data->name);
	lua_pushstring(s, data->url);
	handle_ref(s, lua_ses, data->func_ref, "post dialog function", 3, 1);
}

static int
l_edit_bookmark_dialog(LS)
{
	/* [gettext_accelerator_context(.l_edit_bookmark_dialog)] */
	struct terminal *term = lua_ses->tab->term;
	struct dialog *dlg;
	struct lua_dlg_data *data;

	if (!lua_isstring(S, 1) || !lua_isstring(S, 2)
	    || !lua_isstring(S, 3) || !lua_isfunction(S, 4)) {
		lua_pushnil(S);
		return 1;
	}

#define L_EDIT_BMK_WIDGETS_COUNT 5
	dlg = calloc_dialog(L_EDIT_BMK_WIDGETS_COUNT, sizeof(*data));
	if (!dlg) return 0;

	data = (struct lua_dlg_data *) get_dialog_offset(dlg, L_EDIT_BMK_WIDGETS_COUNT);
	data->state = S;
	safe_strncpy(data->cat, (unsigned char *) lua_tostring(S, 1),
		     MAX_STR_LEN-1);
	safe_strncpy(data->name, (unsigned char *) lua_tostring(S, 2),
		     MAX_STR_LEN-1);
	safe_strncpy(data->url, (unsigned char *) lua_tostring(S, 3),
		     MAX_STR_LEN-1);
	lua_pushvalue(S, 4);
	data->func_ref = luaL_ref(S, LUA_REGISTRYINDEX);

	dlg->title = _("Edit bookmark", term);
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.maximize_width = 1;

	add_dlg_field(dlg, _("Name", term), 0, 0, NULL, MAX_STR_LEN, data->cat, NULL);
	add_dlg_field(dlg, _("Name", term), 0, 0, NULL, MAX_STR_LEN, data->name, NULL);
	add_dlg_field(dlg, _("URL", term), 0, 0, NULL, MAX_STR_LEN, data->url, NULL);

	add_dlg_ok_button(dlg, _("~OK", term), B_ENTER, dialog_run_lua, data);
	add_dlg_button(dlg, _("~Cancel", term), B_ESC, cancel_dialog, NULL);

	add_dlg_end(dlg, L_EDIT_BMK_WIDGETS_COUNT);

	do_dialog(term, dlg, getml(dlg, (void *) NULL));

	lua_pushnumber(S, 1);
	return 1;
}

/* End very hackish bit.  */


/* Begin hackish bit for half-generalised dialog.  */
/* XXX: Add history and custom labels.  */

#define XDIALOG_MAX_FIELDS	5

struct lua_xdialog_data {
	lua_State *state;
	int func_ref;
	int nfields;
	unsigned char fields[XDIALOG_MAX_FIELDS][MAX_STR_LEN];
};

static void
xdialog_run_lua(void *data_)
{
	struct lua_xdialog_data *data = data_;
	lua_State *s = data->state;
	int i;

	for (i = 0; i < data->nfields; i++) lua_pushstring(s, data->fields[i]);
	handle_ref(s, lua_ses, data->func_ref, "post xdialog function",
	           data->nfields, 1);
}

static int
l_xdialog(LS)
{
	/* [gettext_accelerator_context(.l_xdialog)] */
	struct terminal *term;
	struct dialog *dlg;
	struct lua_xdialog_data *data;
	int nargs, nfields, nitems;
	int i = 0;

	if (!lua_ses) return 0;

	term = lua_ses->tab->term;

	nargs = lua_gettop(S);
	nfields = nargs - 1;
	nitems = nfields + 2;

	if ((nfields < 1) || (nfields > XDIALOG_MAX_FIELDS)) goto lua_error;
	for (i = 1; i < nargs; i++) if (!lua_isstring(S, i)) goto lua_error;
	if (!lua_isfunction(S, nargs)) goto lua_error;

	dlg = calloc_dialog(nitems, sizeof(*data));
	if (!dlg) return 0;

	data = (struct lua_xdialog_data *) get_dialog_offset(dlg, nitems);
	data->state = S;
	data->nfields = nfields;
	for (i = 0; i < nfields; i++)
		safe_strncpy(data->fields[i],
			     (unsigned char *) lua_tostring(S, i+1),
			     MAX_STR_LEN-1);
	lua_pushvalue(S, nargs);
	data->func_ref = luaL_ref(S, LUA_REGISTRYINDEX);

	dlg->title = _("User dialog", term);
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.maximize_width = 1;

	for (i = 0; i < nfields; i++)
		add_dlg_field(dlg, _("Name", term), 0, 0, NULL, MAX_STR_LEN,
			      data->fields[i], NULL);

	add_dlg_ok_button(dlg, _("~OK", term), B_ENTER, xdialog_run_lua, data);
	add_dlg_button(dlg, _("~Cancel", term), B_ESC, cancel_dialog, NULL);

	add_dlg_end(dlg, nitems);

	do_dialog(term, dlg, getml(dlg, (void *) NULL));

	lua_pushnumber(S, 1);
	return 1;

lua_error:
	lua_pushnil(S);
	return 1;
}

/* End xdialog bit. */

/* Set/get option */

static int
l_set_option(LS)
{
	int nargs;
	struct option *opt;
	const char *name;

	nargs = lua_gettop(S);
	if (nargs != 2)
		goto lua_error;

	/* Get option record */
	name = lua_tostring(S, 1);
	opt = get_opt_rec(config_options, (unsigned char *) name);
	if (opt == NULL)
		goto lua_error;

	/* Set option */
	switch (opt->type) {
	case OPT_BOOL:
	{
		int value;

		value = lua_toboolean(S, 2);
		option_types[opt->type].set(opt, (unsigned char *) (&value));
		break;
	}
	case OPT_INT:
	{
		int value = lua_tonumber(S, 2);
		option_types[opt->type].set(opt, (unsigned char *) (&value));
		break;
	}
	case OPT_LONG:
	{
		long value = lua_tonumber(S, 2);
		option_types[opt->type].set(opt, (unsigned char *) (&value));
		break;
	}
	case OPT_STRING:
	case OPT_CODEPAGE:
	case OPT_LANGUAGE:
	case OPT_COLOR:
		option_types[opt->type].set(opt, (unsigned char *) lua_tostring(S, 2));
		break;
	default:
		goto lua_error;
	}

	/* Call hook */
	option_changed(lua_ses, opt);
	return 1;

lua_error:
	lua_pushnil(S);
	return 1;
}

static int
l_get_option(LS)
{
	int nargs;
	struct option *opt;
	const char *name;

	/* Get option record */
	nargs = lua_gettop(S);
	if (nargs != 1)
		goto lua_error;
	name = lua_tostring(S, 1);
	opt = get_opt_rec(config_options, (unsigned char *) name);
	if (opt == NULL)
		goto lua_error;

	/* Convert to an appropriate Lua type */
	switch (opt->type) {
	case OPT_BOOL:
		lua_pushboolean(S, opt->value.number);
		break;
	case OPT_INT:
		lua_pushnumber(S, opt->value.number);
		break;
	case OPT_LONG:
		lua_pushnumber(S, opt->value.big_number);
		break;
	case OPT_STRING:
		lua_pushstring(S, opt->value.string);
		break;
	case OPT_CODEPAGE:
	{
		unsigned char *cp_name;

		cp_name = get_cp_config_name(opt->value.number);
		lua_pushstring(S, cp_name);
		break;
	}
	case OPT_LANGUAGE:
	{
		unsigned char *lang;

#ifdef ENABLE_NLS
		lang = language_to_name(current_language);
#else
		lang = "System";
#endif
		lua_pushstring(S, lang);
		break;
	}
	case OPT_COLOR:
	{
		color_T color;
		unsigned char hexcolor[8];
		const unsigned char *strcolor;

		color = opt->value.color;
		strcolor = get_color_string(color, hexcolor);
		lua_pushstring(S, strcolor);
		break;
	}
	case OPT_COMMAND:
	default:
		goto lua_error;
	}

	return 1;

lua_error:
	lua_pushnil(S);
	return 1;
}

/* End of set/get option */

int
eval_function(LS, int num_args, int num_results)
{
	int err;

	err = lua_pcall(S, num_args, num_results, 0);
	if (err) {
		alert_lua_error((unsigned char *) lua_tostring(L, -1));
		lua_pop(L, 1);
	}

	return err;
}

/* Initialisation */

static void
do_hooks_file(LS, unsigned char *prefix, unsigned char *filename)
{
	unsigned char *file = straconcat(prefix, STRING_DIR_SEP, filename,
					 (unsigned char *) NULL);

	if (!file) return;

	/* Test file existence to avoid Lua error reporting (under version 5.x)
	 * Fixes debian bug #231760 ('dbug 231760' using URI rewrite) */
	if (file_can_read(file)) {
		int oldtop = lua_gettop(S);

		if (lua_dofile(S, file) != 0)
			sleep(3); /* Let some time to see error messages. */
		lua_settop(S, oldtop);
	}

	mem_free(file);
}

void
init_lua(struct module *module)
{
	L = lua_open();

	luaopen_base(L);
	luaopen_table(L);
	luaopen_io(L);
	luaopen_string(L);
	luaopen_math(L);

	lua_register(L, LUA_ALERT, l_alert);
	lua_register(L, "current_url", l_current_url);
	lua_register(L, "current_link", l_current_link);
	lua_register(L, "current_title", l_current_title);
	lua_register(L, "current_document", l_current_document);
	lua_register(L, "current_document_formatted", l_current_document_formatted);
	lua_register(L, "pipe_read", l_pipe_read);
	lua_register(L, "execute", l_execute);
	lua_register(L, "tmpname", l_tmpname);
	lua_register(L, "bind_key", l_bind_key);
	lua_register(L, "edit_bookmark_dialog", l_edit_bookmark_dialog);
	lua_register(L, "xdialog", l_xdialog);
	lua_register(L, "set_option", l_set_option);
	lua_register(L, "get_option", l_get_option);

	lua_pushstring(L, elinks_home ? elinks_home
				      : (unsigned char *) CONFDIR);
	lua_setglobal(L, "elinks_home");

	do_hooks_file(L, CONFDIR, LUA_HOOKS_FILENAME);
	if (elinks_home) do_hooks_file(L, elinks_home, LUA_HOOKS_FILENAME);
}

static void free_lua_console_history_entries(void);

void
cleanup_lua(struct module *module)
{
	free_lua_console_history_entries();
	lua_close(L);
}

/* Attempt to handle infinite loops by trapping SIGINT.  If we get a
 * SIGINT, we longjump to where prepare_lua was called.  finish_lua()
 * disables the trapping. */

static void
handle_sigint(void *data)
{
	finish_lua();
	siglongjmp(errjmp, -1);
}

int
prepare_lua(struct session *ses)
{
	lua_ses = ses;
	errterm = lua_ses ? lua_ses->tab->term : NULL;
	/* XXX this uses the wrong term, I think */
	install_signal_handler(SIGINT, (void (*)(void *)) handle_sigint, NULL, 1);

	return sigsetjmp(errjmp, 1);
}

void
finish_lua(void)
{
	/* XXX should save previous handler instead of assuming this one */
	install_signal_handler(SIGINT, (void (*)(void *)) sig_ctrl_c, errterm, 0);
}


/* Error reporting. */

void
alert_lua_error(unsigned char *msg)
{
	if (errterm) {
		info_box(errterm, MSGBOX_NO_TEXT_INTL | MSGBOX_FREE_TEXT,
			N_("Lua Error"), ALIGN_LEFT,
			stracpy(msg));
		return;
	}

	usrerror("Lua: %s", msg);
	sleep(3);
}

void
alert_lua_error2(unsigned char *msg, unsigned char *msg2)
{
	unsigned char *tmp = stracpy(msg);

	if (!tmp) return;
	add_to_strn(&tmp, msg2);
	alert_lua_error(tmp);
	mem_free(tmp);
}


/* The following stuff is to handle the return values of
 * lua_console_hook and keystroke functions, and also the xdialog
 * function.  It expects two values on top of the stack. */

static void
handle_ret_eval(struct session *ses)
{
	const unsigned char *expr = lua_tostring(L, -1);

	if (expr) {
		int oldtop = lua_gettop(L);

		if (prepare_lua(ses) == 0) {
			lua_dostring(L, expr);
			lua_settop(L, oldtop);
			finish_lua();
		}
		return;
	}

	alert_lua_error("bad argument for eval");
}

static void
handle_ret_run(struct session *ses)
{
	unsigned char *cmd = (unsigned char *) lua_tostring(L, -1);

	if (cmd) {
		exec_on_terminal(ses->tab->term, cmd, "", TERM_EXEC_FG);
		return;
	}

	alert_lua_error("bad argument for run");
}

static void
handle_ret_goto_url(struct session *ses)
{
	unsigned char *url = (unsigned char *) lua_tostring(L, -1);

	if (url) {
		goto_url_with_hook(ses, url);
		return;
	}

	alert_lua_error("bad argument for goto_url");
}

static void
handle_standard_lua_returns(unsigned char *from)
{
	const unsigned char *act = lua_tostring(L, -2);

	if (act) {
		if (!strcmp(act, "eval"))
			handle_ret_eval(lua_ses);
		else if (!strcmp(act, "run"))
			handle_ret_run(lua_ses);
		else if (!strcmp(act, "goto_url"))
			handle_ret_goto_url(lua_ses);
		else
			alert_lua_error2("unrecognised return value from ", from);
	}
	else if (!lua_isnil(L, -2))
		alert_lua_error2("bad return type from ", from);

	lua_pop(L, 2);
}

static void
handle_ref_on_stack(LS, struct session *ses, unsigned char *from, int num_args)
{
	int err;

	if (prepare_lua(ses)) return;
	err = eval_function(S, num_args, 2);
	finish_lua();

	if (!err) handle_standard_lua_returns(from);
}

static void
handle_ref(LS, struct session *ses, int func_ref, unsigned char *from,
           int num_args, int unref)
{
	lua_rawgeti(S, LUA_REGISTRYINDEX, func_ref);

	/* The function must be below the arguments on the stack. */
	if (num_args != 0) lua_insert(S, -(num_args + 1));

	handle_ref_on_stack(S, ses, from, num_args);

	if (unref)
		luaL_unref(S, LUA_REGISTRYINDEX, func_ref);
}


/* Console stuff. */

static INIT_INPUT_HISTORY(lua_console_history);

static void
lua_console(struct session *ses, unsigned char *expr)
{
	lua_getglobal(L, "lua_console_hook");
	if (lua_isnil(L, -1)) {
		lua_pop(L, 1);
		handle_ret_eval(ses);
		return;
	}

	lua_pushstring(L, expr);
	handle_ref_on_stack(L, ses, "lua_console_hook", 1);
}

/* TODO: Make this a "Scripting console" instead, with a radiobutton below the
 * inputbox selecting the appropriate scripting backend to use to evaluate the
 * expression. --pasky */

enum evhook_status
dialog_lua_console(va_list ap, void *data)
{
	struct session *ses = va_arg(ap, struct session *);

	if (get_cmd_opt_bool("anonymous"))
		return EVENT_HOOK_STATUS_NEXT;

	input_dialog(ses->tab->term, NULL,
		     N_("Lua Console"), N_("Enter expression"),
		     ses, &lua_console_history,
		     MAX_STR_LEN, "", 0, 0, NULL,
		     (void (*)(void *, unsigned char *)) lua_console, NULL);
	return EVENT_HOOK_STATUS_NEXT;
}

static void
free_lua_console_history_entries(void)
{
	free_list(lua_console_history.entries);
}

enum evhook_status
free_lua_console_history(va_list ap, void *data)
{
	free_lua_console_history_entries();
	return EVENT_HOOK_STATUS_NEXT;
}
