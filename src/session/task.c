/** Sessions task management
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/menu.h"
#include "bfu/dialog.h"
#include "cache/cache.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/html/parser.h"
#include "document/refresh.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "main/event.h"
#include "main/timer.h"
#include "network/connection.h"
#include "osdep/newwin.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "session/download.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "viewer/text/view.h"


static void loading_callback(struct download *, struct session *);

static void
free_task(struct session *ses)
{
	assertm(ses->task.type, "Session has no task");
	if_assert_failed return;

	if (ses->loading_uri) {
		done_uri(ses->loading_uri);
		ses->loading_uri = NULL;
	}
	ses->task.type = TASK_NONE;
	mem_free_set(&ses->task.target.frame, NULL);
}

void
abort_preloading(struct session *ses, int interrupt)
{
	if (!ses->task.type) {
		/* ses->task.target.frame must be freed in the
		 * destroy_session() => abort_loading() =>
		 * abort_preloading() call chain.  Normally,
		 * free_task() called from here would do that,
		 * but if the task has no type, free_task()
		 * cannot be called because an assertion would
		 * fail.  There are several functions that set
		 * ses->task.target.frame without ses->task.type,
		 * so apparently it is allowed.  */
		mem_free_set(&ses->task.target.frame, NULL);
		return;
	}

	cancel_download(&ses->loading, interrupt);
	free_task(ses);
}


struct task {
	struct session *ses;
	struct uri *uri;
	enum cache_mode cache_mode;
	struct session_task session_task;
};

void
ses_load(struct session *ses, struct uri *uri, unsigned char *target_frame,
	 struct location *target_location, enum cache_mode cache_mode,
	 enum task_type task_type)
{
	ses->loading.callback = (download_callback_T *) loading_callback;
	ses->loading.data = ses;
	ses->loading_uri = uri;

	ses->task.type = task_type;
	mem_free_set(&ses->task.target.frame, null_or_stracpy(target_frame));
	ses->task.target.location = target_location;

	load_uri(ses->loading_uri, ses->referrer, &ses->loading,
		 PRI_MAIN, cache_mode, -1);
}

static void
post_yes(void *task_)
{
	struct task *task = task_;

	abort_preloading(task->ses, 0);

	/* XXX: Make the session inherit the URI. */
	ses_load(task->ses, task->uri, task->session_task.target.frame,
	         task->session_task.target.location, task->cache_mode,
	         task->session_task.type);
}

static void
post_no(void *task_)
{
	struct task *task = task_;

	reload(task->ses, CACHE_MODE_NORMAL);
	done_uri(task->uri);
}

/** Check if the URI is obfuscated (bug 382). The problem is said to occur when
 * a URI designed to pass access a specific location with a supplied username,
 * contains misleading chars prior to the @ symbol.
 *
 * An attacker can exploit this issue by supplying a malicious URI pointing to
 * a page designed to mimic that of a trusted site, and tricking a victim who
 * follows a link into believing they are actually at the trusted location.
 *
 * Only the user ID (and not also the password) is checked because only the
 * user ID is displayed in the status bar. */
static int
check_malicious_uri(struct uri *uri)
{
	unsigned char *user, *pos;
	int warn = 0;

	assert(uri->user && uri->userlen);

	user = pos = memacpy(uri->user, uri->userlen);
	if (!user) return 0;

	decode_uri_for_display(user);

	while (*pos) {
		int length, trailing_dots;

		for (length = 0; pos[length] != '\0'; length++)
			if (!(isalnum(pos[length]) || pos[length] == '.'))
				break;

		/* Wind back so that the TLD part is checked correctly. */
		for (trailing_dots = 0; trailing_dots < length; trailing_dots++)
			if (!length || pos[length - trailing_dots - 1] != '.')
				break;

		/* Not perfect, but I am clueless as how to do better. Besides
		 * I don't really think it is an issue for ELinks. --jonas */
		if (end_with_known_tld(pos, length - trailing_dots) != -1) {
			warn = 1;
			break;
		}

		pos += length;

		while (*pos && (!isalnum(*pos) || *pos == '.'))
			pos++;
	}

	mem_free(user);

	return warn;
}

void
ses_goto(struct session *ses, struct uri *uri, unsigned char *target_frame,
	 struct location *target_location, enum cache_mode cache_mode,
	 enum task_type task_type, int redir)
{
	/* [gettext_accelerator_context(ses_goto)] */
	struct task *task;
	int referrer_incomplete = 0;
	int malicious_uri = 0;
	int confirm_submit = uri->form && get_opt_bool("document.browse.forms"
	                                               ".confirm_submit", ses);
	unsigned char *m1 = NULL, *message = NULL;
	struct memory_list *mlist = NULL;

	if (ses->doc_view
	    && ses->doc_view->document
	    && ses->doc_view->document->refresh) {
		kill_document_refresh(ses->doc_view->document->refresh);
	}

	assertm(!ses->loading_uri, "Buggy URI reference counting");

	/* Reset the redirect counter if this is not a redirect. */
	if (!redir) {
		ses->redirect_cnt = 0;
	}

	/* Figure out whether to confirm submit or not */

	/* Only confirm submit if we are posting form data or a misleading URI
	 * was detected. */
	/* Note uri->post might be empty here but we are still supposely
	 * posting form data so this should be more correct. */

	if (uri->user && uri->userlen
	    && get_opt_bool("document.browse.links.warn_malicious", ses)
	    && check_malicious_uri(uri)) {
		malicious_uri = 1;
		confirm_submit = 1;

	} else if (uri->form) {
		/* First check if the referring URI was incomplete. It
		 * indicates that the posted form data might be incomplete too.
		 * See bug 460. */
		if (ses->referrer) {
			struct cache_entry *cached;

			cached = find_in_cache(ses->referrer);
			referrer_incomplete = (cached && cached->incomplete);
		}

		if (referrer_incomplete) {
			confirm_submit = 1;

		} else if (get_validated_cache_entry(uri, cache_mode)) {
			confirm_submit = 0;
		}
	}

	if (!confirm_submit) {
		ses_load(ses, get_uri_reference(uri), target_frame,
		         target_location, cache_mode, task_type);
		return;
	}

	task = mem_alloc(sizeof(*task));
	if (!task) return;

	task->ses = ses;
	task->uri = get_uri_reference(uri);
	task->cache_mode = cache_mode;
	task->session_task.type = task_type;
	task->session_task.target.frame = null_or_stracpy(target_frame);
	task->session_task.target.location = target_location;

	if (malicious_uri) {
		unsigned char *host = memacpy(uri->host, uri->hostlen);
		unsigned char *user = memacpy(uri->user, uri->userlen);
		unsigned char *uristring = get_uri_string(uri, URI_PUBLIC);

		message = msg_text(ses->tab->term,
			N_("The URL you are about to follow might be maliciously "
			"crafted in order to confuse you. By following the URL "
			"you will be connecting to host \"%s\" as user \"%s\".\n\n"
			"Do you want to go to URL %s?"), host, user, uristring);

		mem_free_if(host);
		mem_free_if(user);
		mem_free_if(uristring);

	} else if (redir) {
		m1 = N_("Do you want to follow the redirect and post form data "
			"to URL %s?");

	} else if (referrer_incomplete) {
		m1 = N_("The form data you are about to post might be incomplete.\n"
			"Do you want to post to URL %s?");

	} else if (task_type == TASK_FORWARD) {
		m1 = N_("Do you want to post form data to URL %s?");

	} else {
		m1 = N_("Do you want to repost form data to URL %s?");
	}

	if (!message && m1) {
		unsigned char *uristring = get_uri_string(uri, URI_PUBLIC);

		message = msg_text(ses->tab->term, m1, uristring);
		mem_free_if(uristring);
	}

	add_to_ml(&mlist, task, (void *) NULL);
	if (task->session_task.target.frame)
		add_to_ml(&mlist, task->session_task.target.frame, (void *) NULL);
	msg_box(ses->tab->term, mlist, MSGBOX_FREE_TEXT,
		N_("Warning"), ALIGN_CENTER,
		message,
		task, 2,
		MSG_BOX_BUTTON(N_("~Yes"), post_yes, B_ENTER),
		MSG_BOX_BUTTON(N_("~No"), post_no, B_ESC));
}


/** If @a loaded_in_frame is set, this was called just to indicate a move inside
 * a frameset, and we basically just reset the appropriate frame's view_state in
 * that case. When clicking on a link inside a frame, the frame URI is somehow
 * updated and added to the files-to-load queue, then ses_forward() is called
 * with @a loaded_in_frame unset, duplicating the whole frameset's location,
 * then later the file-to-load callback calls it for the particular frame with
 * @a loaded_in_frame set. */
struct view_state *
ses_forward(struct session *ses, int loaded_in_frame)
{
	struct location *loc = NULL;
	struct view_state *vs;

	if (!loaded_in_frame) {
		free_files(ses);
		mem_free_set(&ses->search_word, NULL);
	}

x:
	if (!loaded_in_frame) {
		loc = mem_calloc(1, sizeof(*loc));
		if (!loc) return NULL;
		copy_struct(&loc->download, &ses->loading);
	}

	if (ses->task.target.frame && *ses->task.target.frame) {
		struct frame *frame;

		assertm(have_location(ses), "no location yet");
		if_assert_failed return NULL;

		if (!loaded_in_frame) {
			copy_location(loc, cur_loc(ses));
			add_to_history(&ses->history, loc);
		}

		frame = ses_find_frame(ses, ses->task.target.frame);
		if (!frame) {
			if (!loaded_in_frame) {
				del_from_history(&ses->history, loc);
				destroy_location(loc);
			}
			mem_free_set(&ses->task.target.frame, NULL);
			goto x;
		}

		vs = &frame->vs;
		if (!loaded_in_frame) {
			destroy_vs(vs, 1);
			init_vs(vs, ses->loading_uri, vs->plain);
		} else {
			done_uri(vs->uri);
			vs->uri = get_uri_reference(ses->loading_uri);
			if (vs->doc_view) {
				/* vs->doc_view itself will get detached in
				 * render_document_frames(), but that's too
				 * late for us. */
				vs->doc_view->vs = NULL;
				vs->doc_view = NULL;
			}
#ifdef CONFIG_ECMASCRIPT
			vs->ecmascript_fragile = 1;
#endif
		}

	} else {
		assert(loc);
		if_assert_failed return NULL;

		init_list(loc->frames);
		vs = &loc->vs;
		init_vs(vs, ses->loading_uri, vs->plain);
		add_to_history(&ses->history, loc);
	}

	ses->status.visited = 0;

	/* This is another "branch" in the browsing, so throw away the current
	 * unhistory, we are venturing in another direction! */
	if (ses->task.type == TASK_FORWARD)
		clean_unhistory(&ses->history);
	return vs;
}

static void
ses_imgmap(struct session *ses)
{
	struct cache_entry *cached = find_in_cache(ses->loading_uri);
	struct document_view *doc_view = current_frame(ses);
	struct fragment *fragment;
	struct memory_list *ml;
	struct menu_item *menu;

	if (!cached) {
		INTERNAL("can't find cache entry");
		return;
	}

	fragment = get_cache_fragment(cached);
	if (!fragment) return;

	if (!doc_view || !doc_view->document) return;

	if (get_image_map(cached->head, fragment->data,
			  fragment->data + fragment->length,
			  &menu, &ml, ses->loading_uri,
			  &doc_view->document->options,
			  ses->task.target.frame,
			  get_terminal_codepage(ses->tab->term),
			  get_opt_codepage("document.codepage.assume", ses),
			  get_opt_bool("document.codepage.force_assumed", ses)))
		return;

	add_empty_window(ses->tab->term, (void (*)(void *)) freeml, ml);
	do_menu(ses->tab->term, menu, ses, 0);
}

enum do_move {
	DO_MOVE_ABORT,
	DO_MOVE_DISPLAY,
	DO_MOVE_DONE
};

static enum do_move
do_redirect(struct session *ses, struct download **download_p, struct cache_entry *cached)
{
	enum task_type task = ses->task.type;

	if (task == TASK_HISTORY && !have_location(ses))
		return DO_MOVE_DISPLAY;

	assertm(compare_uri(cached->uri, ses->loading_uri, URI_BASE),
		"Redirecting using bad base URI");

	if (cached->redirect->protocol == PROTOCOL_UNKNOWN)
		return DO_MOVE_ABORT;

	abort_loading(ses, 0);
	if (have_location(ses))
		*download_p = &cur_loc(ses)->download;
	else
		*download_p = NULL;

	set_session_referrer(ses, cached->uri);

	switch (task) {
	case TASK_NONE:
		break;
	case TASK_FORWARD:
	{
		protocol_external_handler_T *fn;
		struct uri *uri = cached->redirect;

		fn = get_protocol_external_handler(ses->tab->term, uri);
		if (fn) {
			fn(ses, uri);
			*download_p = NULL;
			return DO_MOVE_ABORT;
		}
	}
	/* Fall through. */
	case TASK_IMGMAP:
		ses_goto(ses, cached->redirect, ses->task.target.frame, NULL,
			 CACHE_MODE_NORMAL, task, 1);
		return DO_MOVE_DONE;
	case TASK_HISTORY:
		ses_goto(ses, cached->redirect, NULL, ses->task.target.location,
			 CACHE_MODE_NORMAL, TASK_RELOAD, 1);
		return DO_MOVE_DONE;
	case TASK_RELOAD:
		ses_goto(ses, cached->redirect, NULL, NULL,
			 ses->reloadlevel, TASK_RELOAD, 1);
		return DO_MOVE_DONE;
	}

	return DO_MOVE_DISPLAY;
}

static enum do_move
do_move(struct session *ses, struct download **download_p)
{
	struct cache_entry *cached;

	assert(download_p && *download_p);
	assertm(ses->loading_uri != NULL, "no ses->loading_uri");
	if_assert_failed return DO_MOVE_ABORT;

	if (ses->loading_uri->protocol == PROTOCOL_UNKNOWN)
		return DO_MOVE_ABORT;

	/* Handling image map needs to scan the source of the loaded document
	 * so all of it has to be available. */
	if (ses->task.type == TASK_IMGMAP && is_in_progress_state((*download_p)->state))
		return DO_MOVE_ABORT;

	cached = (*download_p)->cached;
	if (!cached) return DO_MOVE_ABORT;

	if (cached->redirect && ses->redirect_cnt++ < MAX_REDIRECTS) {
		enum do_move d = do_redirect(ses, download_p, cached);

		if (d != DO_MOVE_DISPLAY) return d;
	}

	kill_timer(&ses->display_timer);

	switch (ses->task.type) {
		case TASK_NONE:
			break;
		case TASK_FORWARD:
			if (setup_download_handler(ses, &ses->loading, cached, 0)) {
				free_task(ses);
				reload(ses, CACHE_MODE_NORMAL);
				return DO_MOVE_DONE;
			}
			break;
		case TASK_IMGMAP:
			ses_imgmap(ses);
			break;
		case TASK_HISTORY:
			ses_history_move(ses);
			break;
		case TASK_RELOAD:
			ses->task.target.location = cur_loc(ses)->prev;
			ses_history_move(ses);
			ses_forward(ses, 0);
			break;
	}

	if (is_in_progress_state((*download_p)->state)) {
		if (have_location(ses))
			*download_p = &cur_loc(ses)->download;
		move_download(&ses->loading, *download_p, PRI_MAIN);
	} else if (have_location(ses)) {
		cur_loc(ses)->download.state = ses->loading.state;
	}

	free_task(ses);
	return DO_MOVE_DISPLAY;
}

static void
loading_callback(struct download *download, struct session *ses)
{
	enum do_move d;

	assertm(ses->task.type, "loading_callback: no ses->task");
	if_assert_failed return;

	d = do_move(ses, &download);
	if (!download) return;
	if (d == DO_MOVE_DONE) goto end;

	if (d == DO_MOVE_DISPLAY) {
		download->callback = (download_callback_T *) doc_loading_callback;
		download->data = ses;
		display_timer(ses);
	}

	if (is_in_result_state(download->state)) {
		if (ses->task.type) free_task(ses);
		if (d == DO_MOVE_DISPLAY) doc_loading_callback(download, ses);
	}

	if (is_in_result_state(download->state)
	    && !is_in_state(download->state, S_OK)) {
		print_error_dialog(ses, download->state,
				   download->conn ? download->conn->uri : NULL,
				   download->pri);
		if (d == DO_MOVE_ABORT) reload(ses, CACHE_MODE_NORMAL);
	}

end:
	check_questions_queue(ses);
	print_screen_status(ses);
}


static void
do_follow_url(struct session *ses, struct uri *uri, unsigned char *target,
	      enum task_type task, enum cache_mode cache_mode, int do_referrer)
{
	struct uri *referrer = NULL;
	protocol_external_handler_T *external_handler;

	if (!uri) {
		print_error_dialog(ses, connection_state(S_BAD_URL), uri, PRI_CANCEL);
		return;
	}

	external_handler = get_protocol_external_handler(ses->tab->term, uri);
	if (external_handler) {
		external_handler(ses, uri);
		return;
	}

	if (do_referrer) {
		struct document_view *doc_view = current_frame(ses);

		if (doc_view && doc_view->document)
			referrer = doc_view->document->uri;
	}

	if (target && !strcmp(target, "_blank")) {
		int mode = get_opt_int("document.browse.links.target_blank",
		                       ses);

		if (mode == 3
		    && !get_cmd_opt_bool("anonymous")
		    && can_open_in_new(ses->tab->term)
		    && !get_cmd_opt_bool("no-connect")
		    && !get_cmd_opt_bool("no-home")) {
			enum term_env_type env = ses->tab->term->environment;

			open_uri_in_new_window(ses, uri, referrer, env,
					       cache_mode, task);
			return;
		}

		if (mode > 0) {
			struct session *new_ses;

			new_ses = init_session(ses, ses->tab->term, uri, (mode == 2));
			if (new_ses) ses = new_ses;
		}
	}

	ses->reloadlevel = cache_mode;

	if (ses->task.type == task) {
		if (compare_uri(ses->loading_uri, uri, 0)) {
			/* We're already loading the URL. */
			return;
		}
	}

	abort_loading(ses, 0);

	set_session_referrer(ses, referrer);

	ses_goto(ses, uri, target, NULL, cache_mode, task, 0);
}

static void
follow_url(struct session *ses, struct uri *uri, unsigned char *target,
	   enum task_type task, enum cache_mode cache_mode, int referrer)
{
#ifdef CONFIG_SCRIPTING
	static int follow_url_event_id = EVENT_NONE;
	unsigned char *uristring;

	uristring = uri && !uri->post ? get_uri_string(uri, URI_BASE | URI_FRAGMENT)
	                              : NULL;

	/* Do nothing if we do not have a URI or if it is a POST request
	 * because scripts can corrupt POST requests leading to bad
	 * things happening later on. */
	if (!uristring) {
		do_follow_url(ses, uri, target, task, cache_mode, referrer);
		return;
	}

	set_event_id(follow_url_event_id, "follow-url");
	trigger_event(follow_url_event_id, &uristring, ses);

	if (!uristring || !*uristring) {
		mem_free_if(uristring);
		return;
	}

	/* FIXME: Compare if uristring and struri(uri) are equal */
	/* FIXME: When uri->post will no longer be an encoded string (but
	 * hopefully some refcounted object) we will have to assign the post
	 * data object to the translated URI. */
	uri = get_translated_uri(uristring, ses->tab->term->cwd);
	mem_free(uristring);
#endif

	do_follow_url(ses, uri, target, task, cache_mode, referrer);

#ifdef CONFIG_SCRIPTING
	if (uri) done_uri(uri);
#endif
}

void
goto_uri(struct session *ses, struct uri *uri)
{
	follow_url(ses, uri, NULL, TASK_FORWARD, CACHE_MODE_NORMAL, 0);
}

void
goto_uri_frame(struct session *ses, struct uri *uri,
	       unsigned char *target, enum cache_mode cache_mode)
{
	follow_url(ses, uri, target, TASK_FORWARD, cache_mode, 1);
}

void
delayed_goto_uri_frame(void *data)
{
	struct delayed_open *deo = data;
	struct frame *frame;

	assert(deo);
	frame = deo->target ? ses_find_frame(deo->ses, deo->target) : NULL;
	if (frame)
		goto_uri_frame(deo->ses, deo->uri, frame->name, CACHE_MODE_NORMAL);
	else {
		goto_uri_frame(deo->ses, deo->uri, NULL, CACHE_MODE_NORMAL);
	}
	done_uri(deo->uri);
	mem_free_if(deo->target);
	mem_free(deo);
}

/* menu_func_T */
void
map_selected(struct terminal *term, void *ld_, void *ses_)
{
	struct link_def *ld = ld_;
	struct session *ses = ses_;
	struct uri *uri = get_uri(ld->link, 0);

	goto_uri_frame(ses, uri, ld->target, CACHE_MODE_NORMAL);
	if (uri) done_uri(uri);
}


void
goto_url(struct session *ses, unsigned char *url)
{
	struct uri *uri = get_uri(url, 0);

	goto_uri(ses, uri);
	if (uri) done_uri(uri);
}

struct uri *
get_hooked_uri(unsigned char *uristring, struct session *ses, unsigned char *cwd)
{
	struct uri *uri;

#if defined(CONFIG_SCRIPTING) || defined(CONFIG_URI_REWRITE)
	static int goto_url_event_id = EVENT_NONE;

	uristring = stracpy(uristring);
	if (!uristring) return NULL;

	set_event_id(goto_url_event_id, "goto-url");
	trigger_event(goto_url_event_id, &uristring, ses);
	if (!uristring) return NULL;
#endif

	uri = *uristring ? get_translated_uri(uristring, cwd) : NULL;

#if defined(CONFIG_SCRIPTING) || defined(CONFIG_URI_REWRITE)
	mem_free(uristring);
#endif
	return uri;
}

void
goto_url_with_hook(struct session *ses, unsigned char *url)
{
	unsigned char *cwd = ses->tab->term->cwd;
	struct uri *uri;

	/* Bail out if passed empty string from goto-url dialog */
	if (!*url) return;

	uri = get_hooked_uri(url, ses, cwd);
	if (!uri) return;

	goto_uri(ses, uri);

	done_uri(uri);
}

int
goto_url_home(struct session *ses)
{
	unsigned char *homepage = get_opt_str("ui.sessions.homepage", ses);

	if (!*homepage) homepage = getenv("WWW_HOME");
	if (!homepage || !*homepage) homepage = WWW_HOME_URL;

	if (!homepage || !*homepage) return 0;

	goto_url_with_hook(ses, homepage);
	return 1;
}

/* TODO: Should there be goto_imgmap_reload() ? */

void
goto_imgmap(struct session *ses, struct uri *uri, unsigned char *target)
{
	follow_url(ses, uri, target, TASK_IMGMAP, CACHE_MODE_NORMAL, 1);
}
