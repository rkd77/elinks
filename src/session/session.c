/** Sessions managment - you'll find things here which you wouldn't expect
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bookmarks/bookmarks.h"
#include "cache/cache.h"
#include "config/home.h"
#include "config/options.h"
#include "dialogs/menu.h"
#include "dialogs/status.h"
#include "document/html/frames.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/refresh.h"
#include "document/renderer.h"
#include "document/view.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "main/event.h"
#include "main/object.h"
#include "main/timer.h"
#include "network/connection.h"
#include "network/state.h"
#include "osdep/newwin.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/time.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"


struct file_to_load {
	LIST_HEAD(struct file_to_load);

	struct session *ses;
	unsigned int req_sent:1;
	int pri;
	struct cache_entry *cached;
	unsigned char *target_frame;
	struct uri *uri;
	struct download download;
};

/** This structure and related functions are used to maintain information
 * for instances opened in new windows. We store all related session info like
 * URI and base session to clone from so that when the new instance connects
 * we can look up this information. In case of failure the session information
 * has a timeout */
struct session_info {
	LIST_HEAD(struct session_info);

	int id;
	timer_id_T timer;
	struct session *ses;
	struct uri *uri;
	struct uri *referrer;
	enum task_type task;
	enum cache_mode cache_mode;
};

#define file_to_load_is_active(ftl) ((ftl)->req_sent && is_in_progress_state((ftl)->download.state))


INIT_LIST_OF(struct session, sessions);

enum remote_session_flags remote_session_flags;


static struct file_to_load *request_additional_file(struct session *,
						    unsigned char *,
						    struct uri *, int);

static window_handler_T tabwin_func;


static INIT_LIST_OF(struct session_info, session_info);
static int session_info_id = 1;

static struct session_info *
get_session_info(int id)
{
	struct session_info *info;

	foreach (info, session_info) {
		struct session *ses;

		if (info->id != id) continue;

		/* Make sure the info->ses session is still with us. */
		foreach (ses, sessions)
			if (ses == info->ses)
				return info;

		info->ses = NULL;
		return info;
	}

	return NULL;
}

static void
done_session_info(struct session_info *info)
{
	del_from_list(info);
	kill_timer(&info->timer);

	if (info->uri) done_uri(info->uri);
	if (info->referrer) done_uri(info->referrer);
	mem_free(info);
}

void
done_saved_session_info(void)
{
	while (!list_empty(session_info))
		done_session_info(session_info.next);
}

/** Timer callback for session_info.timer.  As explained in install_timer(),
 * this function must erase the expired timer ID from all variables.  */
static void
session_info_timeout(int id)
{
	struct session_info *info = get_session_info(id);

	if (!info) return;
	info->timer = TIMER_ID_UNDEF;
	/* The expired timer ID has now been erased.  */
	done_session_info(info);
}

int
add_session_info(struct session *ses, struct uri *uri, struct uri *referrer,
		 enum cache_mode cache_mode, enum task_type task)
{
	struct session_info *info = mem_calloc(1, sizeof(*info));

	if (!info) return -1;

	info->id = session_info_id++;
	/* I don't know what a reasonable start up time for a new instance is
	 * but it won't hurt to have a few seconds atleast. --jonas */
	install_timer(&info->timer, (milliseconds_T) 10000,
		      (void (*)(void *)) session_info_timeout,
		      (void *) (long) info->id);

	info->ses = ses;
	info->task = task;
	info->cache_mode = cache_mode;

	if (uri) info->uri = get_uri_reference(uri);
	if (referrer) info->referrer = get_uri_reference(referrer);

	add_to_list(session_info, info);

	return info->id;
}

static struct session *
init_saved_session(struct terminal *term, int id)
{
	struct session_info *info = get_session_info(id);
	struct session *ses;

	if (!info) return NULL;

	ses = init_session(info->ses, term, info->uri, 0);

	if (!ses) {
		done_session_info(info);
		return ses;
	}

	/* Only do the ses_goto()-thing for target=_blank. */
	if (info->uri && info->task != TASK_NONE) {
		/* The init_session() call would have started the download but
		 * not with the requested cache mode etc. so interrupt that
		 * download . */
		abort_loading(ses, 1);

		ses->reloadlevel = info->cache_mode;
		set_session_referrer(ses, info->referrer);
		ses_goto(ses, info->uri, NULL, NULL, info->cache_mode,
			 info->task, 0);
	}

	done_session_info(info);

	return ses;
}

static struct session *
get_master_session(void)
{
	struct session *ses;

	foreach (ses, sessions)
		if (ses->tab->term->master) {
			struct window *current_tab = get_current_tab(ses->tab->term);

			return current_tab ? current_tab->data : NULL;
		}

	return NULL;
}

/** @relates session */
struct download *
get_current_download(struct session *ses)
{
	struct download *download = NULL;

	if (!ses) return NULL;

	if (ses->task.type)
		download = &ses->loading;
	else if (have_location(ses))
		download = &cur_loc(ses)->download;

	if (download && is_in_state(download->state, S_OK)) {
		struct file_to_load *ftl;

		foreach (ftl, ses->more_files)
			if (file_to_load_is_active(ftl))
				return &ftl->download;
	}

	/* Note that @download isn't necessarily NULL here,
	 * if @ses->more_files is empty. -- Miciah */
	return download;
}

void
print_error_dialog(struct session *ses, struct connection_state state,
		   struct uri *uri, enum connection_priority priority)
{
	struct string msg;
	unsigned char *uristring;

	/* Don't show error dialogs for missing CSS stylesheets */
	if (priority == PRI_CSS
	    || !init_string(&msg))
		return;

	uristring = uri ? get_uri_string(uri, URI_PUBLIC) : NULL;
	if (uristring) {
#ifdef CONFIG_UTF8
		if (ses->tab->term->utf8_cp)
			decode_uri(uristring);
		else
#endif /* CONFIG_UTF8 */
			decode_uri_for_display(uristring);
		add_format_to_string(&msg,
			_("Unable to retrieve %s", ses->tab->term),
			uristring);
		mem_free(uristring);
		add_to_string(&msg, ":\n\n");
	}

	add_to_string(&msg, get_state_message(state, ses->tab->term));

	info_box(ses->tab->term, MSGBOX_FREE_TEXT,
		 N_("Error"), ALIGN_CENTER,
		 msg.source);

	/* TODO: retry */
}

static void
abort_files_load(struct session *ses, int interrupt)
{
	while (1) {
		struct file_to_load *ftl;
		int more = 0;

		foreach (ftl, ses->more_files) {
			if (!file_to_load_is_active(ftl))
				continue;

			more = 1;
			cancel_download(&ftl->download, interrupt);
		}

		if (!more) break;
	};
}

void
free_files(struct session *ses)
{
	struct file_to_load *ftl;

	abort_files_load(ses, 0);
	foreach (ftl, ses->more_files) {
		if (ftl->cached) object_unlock(ftl->cached);
		if (ftl->uri) done_uri(ftl->uri);
		mem_free_if(ftl->target_frame);
	}
	free_list(ses->more_files);
}




static void request_frameset(struct session *, struct frameset_desc *, int);

static void
request_frame(struct session *ses, unsigned char *name,
	      struct uri *uri, int depth)
{
	struct location *loc = cur_loc(ses);
	struct frame *frame;

	assertm(have_location(ses), "request_frame: no location");
	if_assert_failed return;

	foreach (frame, loc->frames) {
		struct document_view *doc_view;

		if (c_strcasecmp(frame->name, name))
			continue;

		foreach (doc_view, ses->scrn_frames) {
			if (doc_view->vs == &frame->vs && doc_view->document->frame_desc) {
				request_frameset(ses, doc_view->document->frame_desc, depth);
				return;
			}
		}

		request_additional_file(ses, name, frame->vs.uri, PRI_FRAME);
		return;
	}

	frame = mem_calloc(1, sizeof(*frame));
	if (!frame) return;

	frame->name = stracpy(name);
	if (!frame->name) {
		mem_free(frame);
		return;
	}

	init_vs(&frame->vs, uri, -1);

	add_to_list(loc->frames, frame);

	request_additional_file(ses, name, frame->vs.uri, PRI_FRAME);
}

static void
request_frameset(struct session *ses, struct frameset_desc *frameset_desc, int depth)
{
	int i;

	if (depth > HTML_MAX_FRAME_DEPTH) return;

	depth++; /* Inheritation counter (recursion brake ;) */

	for (i = 0; i < frameset_desc->n; i++) {
		struct frame_desc *frame_desc = &frameset_desc->frame_desc[i];

		if (frame_desc->subframe) {
			request_frameset(ses, frame_desc->subframe, depth);
		} else if (frame_desc->name && frame_desc->uri) {
			request_frame(ses, frame_desc->name,
				      frame_desc->uri, depth);
		}
	}
}

#ifdef CONFIG_CSS
static inline void
load_css_imports(struct session *ses, struct document_view *doc_view)
{
	struct document *document = doc_view->document;
	struct uri *uri;
	int index;

	if (!document) return;

	foreach_uri (uri, index, &document->css_imports) {
		request_additional_file(ses, "", uri, PRI_CSS);
	}
}
#else
#define load_css_imports(ses, doc_view)
#endif

#ifdef CONFIG_ECMASCRIPT
static inline void
load_ecmascript_imports(struct session *ses, struct document_view *doc_view)
{
	struct document *document = doc_view->document;
	struct uri *uri;
	int index;

	if (!document) return;

	foreach_uri (uri, index, &document->ecmascript_imports) {
		request_additional_file(ses, "", uri, /* XXX */ PRI_CSS);
	}
}
#else
#define load_ecmascript_imports(ses, doc_view)
#endif

NONSTATIC_INLINE void
load_frames(struct session *ses, struct document_view *doc_view)
{
	struct document *document = doc_view->document;

	if (!document || !document->frame_desc) return;
	request_frameset(ses, document->frame_desc, 0);

	foreach (doc_view, ses->scrn_frames) {
		load_css_imports(ses, doc_view);
		load_ecmascript_imports(ses, doc_view);
	}
}

/** Timer callback for session.display_timer.  As explained in install_timer(),
 * this function must erase the expired timer ID from all variables.  */
void
display_timer(struct session *ses)
{
	timeval_T start, stop, duration;
	milliseconds_T t;

	timeval_now(&start);
	draw_formatted(ses, 3);
	timeval_now(&stop);
	timeval_sub(&duration, &start, &stop);

	t = mult_ms(timeval_to_milliseconds(&duration), DISPLAY_TIME);
	if (t < DISPLAY_TIME_MIN) t = DISPLAY_TIME_MIN;
	install_timer(&ses->display_timer, t,
		      (void (*)(void *)) display_timer,
		      ses);
	/* The expired timer ID has now been erased.  */

	load_frames(ses, ses->doc_view);
	load_css_imports(ses, ses->doc_view);
	load_ecmascript_imports(ses, ses->doc_view);
	process_file_requests(ses);
}


struct questions_entry {
	LIST_HEAD(struct questions_entry);

	void (*callback)(struct session *, void *);
	void *data;
};

INIT_LIST_OF(struct questions_entry, questions_queue);


void
check_questions_queue(struct session *ses)
{
	while (!list_empty(questions_queue)) {
		struct questions_entry *q = questions_queue.next;

		q->callback(ses, q->data);
		del_from_list(q);
		mem_free(q);
	}
}

void
add_questions_entry(void (*callback)(struct session *, void *), void *data)
{
	struct questions_entry *q = mem_alloc(sizeof(*q));

	if (!q) return;

	q->callback = callback;
	q->data	    = data;
	add_to_list(questions_queue, q);
}

#ifdef CONFIG_SCRIPTING
static void
maybe_pre_format_html(struct cache_entry *cached, struct session *ses)
{
	struct fragment *fragment;
	static int pre_format_html_event = EVENT_NONE;

	if (!cached || cached->preformatted)
		return;

	/* The script called from here may indirectly call
	 * garbage_collection().  If the refcount of the cache entry
	 * were 0, it could then be freed, and the
	 * cached->preformatted assignment at the end of this function
	 * would crash.  Normally, the document has a reference to the
	 * cache entry, and that suffices.  However, if the cache
	 * entry was loaded to satisfy e.g. USEMAP="imgmap.html#map",
	 * then cached->object.refcount == 0 here, and must be
	 * incremented.
	 * 
	 * cached->object.refcount == 0 is safe while the cache entry
	 * is being loaded, because garbage_collection() calls
	 * is_entry_used(), which checks whether any connection is
	 * using the cache entry.  But loading has ended before this
	 * point.  */
	object_lock(cached);

	fragment = get_cache_fragment(cached);
	if (!fragment) goto unlock_and_return;

	/* We cannot do anything if the data are fragmented. */
	if (!list_is_singleton(cached->frag)) goto unlock_and_return;

	set_event_id(pre_format_html_event, "pre-format-html");
	trigger_event(pre_format_html_event, ses, cached);

	/* XXX: Keep this after the trigger_event, because hooks might call
	 * normalize_cache_entry()! */
	cached->preformatted = 1;

unlock_and_return:
	object_unlock(cached);
}
#endif

static int
check_incomplete_redirects(struct cache_entry *cached)
{
	assert(cached);

	cached = follow_cached_redirects(cached);
	if (cached && !cached->redirect) {
		/* XXX: This is not quite true, but does that difference
		 * matter here? */
		return cached->incomplete;
	}

	return 0;
}

int
session_is_loading(struct session *ses)
{
	struct download *download = get_current_download(ses);

	if (!download) return 0;

	if (!is_in_result_state(download->state))
		return 1;

	/* The validness of download->cached (especially the download struct in
	 * ses->loading) is hard to maintain so check before using it.
	 * Related to bug 559. */
	if (download->cached
	    && cache_entry_is_valid(download->cached)
	    && check_incomplete_redirects(download->cached))
		return 1;

	return 0;
}

void
doc_loading_callback(struct download *download, struct session *ses)
{
	int submit = 0;

	if (is_in_result_state(download->state)) {
#ifdef CONFIG_SCRIPTING
		maybe_pre_format_html(download->cached, ses);
#endif
		kill_timer(&ses->display_timer);

		draw_formatted(ses, 1);

		if (get_cmd_opt_bool("auto-submit")) {
			if (!list_empty(ses->doc_view->document->forms)) {
				get_cmd_opt_bool("auto-submit") = 0;
				submit = 1;
			}
		}

		load_frames(ses, ses->doc_view);
		load_css_imports(ses, ses->doc_view);
		load_ecmascript_imports(ses, ses->doc_view);
		process_file_requests(ses);

		start_document_refreshes(ses);

		if (!is_in_state(download->state, S_OK)) {
			print_error_dialog(ses, download->state,
					   ses->doc_view->document->uri,
					   download->pri);
		}

	} else if (is_in_transfering_state(download->state)
	           && ses->display_timer == TIMER_ID_UNDEF) {
		display_timer(ses);
	}

	check_questions_queue(ses);
	print_screen_status(ses);

#ifdef CONFIG_GLOBHIST
	if (download->pri != PRI_CSS) {
		unsigned char *title = ses->doc_view->document->title;
		struct uri *uri;

		if (download->conn)
			uri = download->conn->proxied_uri;
		else if (download->cached)
			uri = download->cached->uri;
		else
			uri = NULL;

		if (uri)
			add_global_history_item(struri(uri), title, time(NULL));
	}
#endif

	if (submit) auto_submit_form(ses);
}

static void
file_loading_callback(struct download *download, struct file_to_load *ftl)
{
	if (ftl->download.cached && ftl->cached != ftl->download.cached) {
		if (ftl->cached) object_unlock(ftl->cached);
		ftl->cached = ftl->download.cached;
		object_lock(ftl->cached);
	}

	/* FIXME: We need to do content-type check here! However, we won't
	 * handle properly the "Choose action" dialog now :(. */
	if (ftl->cached && !ftl->cached->redirect_get && download->pri != PRI_CSS) {
		struct session *ses = ftl->ses;
		struct uri *loading_uri = ses->loading_uri;
		unsigned char *target_frame = ses->task.target.frame;

		ses->loading_uri = ftl->uri;
		ses->task.target.frame = ftl->target_frame;
		setup_download_handler(ses, &ftl->download, ftl->cached, 1);
		ses->loading_uri = loading_uri;
		ses->task.target.frame = target_frame;
	}

	doc_loading_callback(download, ftl->ses);
}

static struct file_to_load *
request_additional_file(struct session *ses, unsigned char *name, struct uri *uri, int pri)
{
	struct file_to_load *ftl;

	if (uri->protocol == PROTOCOL_UNKNOWN) {
		return NULL;
	}

	/* XXX: We cannot run the external handler here, because
	 * request_additional_file() is called many times for a single URL
	 * (normally the foreach() right below catches them all). Anyway,
	 * having <frame src="mailto:foo"> would be just weird, wouldn't it?
	 * --pasky */
	if (get_protocol_external_handler(ses->tab->term, uri)) {
		return NULL;
	}

	foreach (ftl, ses->more_files) {
		if (compare_uri(ftl->uri, uri, URI_BASE)) {
			if (ftl->pri > pri) {
				ftl->pri = pri;
				move_download(&ftl->download, &ftl->download, pri);
			}
			return NULL;
		}
	}

	ftl = mem_calloc(1, sizeof(*ftl));
	if (!ftl) return NULL;

	ftl->uri = get_uri_reference(uri);
	ftl->target_frame = stracpy(name);
	ftl->download.callback = (download_callback_T *) file_loading_callback;
	ftl->download.data = ftl;
	ftl->pri = pri;
	ftl->ses = ses;

	add_to_list(ses->more_files, ftl);

	return ftl;
}

static void
load_additional_file(struct file_to_load *ftl, enum cache_mode cache_mode)
{
	struct document_view *doc_view = current_frame(ftl->ses);
	struct uri *referrer = doc_view && doc_view->document
			     ? doc_view->document->uri : NULL;

	load_uri(ftl->uri, referrer, &ftl->download, ftl->pri, cache_mode, -1);
}

void
process_file_requests(struct session *ses)
{
	if (ses->status.processing_file_requests) return;
	ses->status.processing_file_requests = 1;

	while (1) {
		struct file_to_load *ftl;
		int more = 0;

		foreach (ftl, ses->more_files) {
			if (ftl->req_sent)
				continue;

			ftl->req_sent = 1;

			load_additional_file(ftl, CACHE_MODE_NORMAL);
			more = 1;
		}

		if (!more) break;
	};

	ses->status.processing_file_requests = 0;
}


static void
dialog_goto_url_open(void *data)
{
	dialog_goto_url((struct session *) data, NULL);
}

/** @returns 0 if the first session was not properly initialized and
 * setup_session() should be called on the session as well. */
static int
setup_first_session(struct session *ses, struct uri *uri)
{
	/* [gettext_accelerator_context(setup_first_session)] */
	struct terminal *term = ses->tab->term;

	if (!*get_opt_str("protocol.http.user_agent")) {
		info_box(term, 0,
			 N_("Warning"), ALIGN_CENTER,
			 N_("You have an empty string in protocol.http.user_agent - "
			 "this was a default value in the past, substituted by "
			 "default ELinks User-Agent string. However, currently "
			 "this means that NO User-Agent HEADER "
		 	 "WILL BE SENT AT ALL - if this is really what you want, "
			 "set its value to \" \", otherwise please delete the line "
			 "with this setting from your configuration file (if you "
			 "have no idea what I'm talking about, just do this), so "
			 "that the correct default setting will be used. Apologies for "
			 "any inconvience caused."));
	}

	if (!get_opt_bool("config.saving_style_w")) {
		struct option *opt = get_opt_rec(config_options, "config.saving_style_w");
		opt->value.number = 1;
		option_changed(ses, opt);
		if (get_opt_int("config.saving_style") != 3) {
			info_box(term, 0,
				 N_("Warning"), ALIGN_CENTER,
				 N_("You have option config.saving_style set to "
				 "a de facto obsolete value. The configuration "
				 "saving algorithms of ELinks were changed from "
				 "the last time you upgraded ELinks. Now, only "
				 "those options which you actually changed are "
				 "saved to the configuration file, instead of "
				 "all the options. This simplifies our "
				 "situation greatly when we see that some option "
				 "has an inappropriate default value or we need to "
				 "change the semantics of some option in a subtle way. "
				 "Thus, we recommend you change the value of "
				 "config.saving_style option to 3 in order to get "
				 "the \"right\" behaviour. Apologies for any "
				 "inconvience caused."));
		}
	}

	if (first_use) {
		/* Only open the goto URL dialog if no URI was passed on the
		 * command line. */
		void *handler = uri ? NULL : dialog_goto_url_open;

		first_use = 0;

		msg_box(term, NULL, 0,
			N_("Welcome"), ALIGN_CENTER,
			N_("Welcome to ELinks!\n\n"
			"Press ESC for menu. Documentation is available in "
			"Help menu."),
			ses, 1,
			MSG_BOX_BUTTON(N_("~OK"), handler, B_ENTER | B_ESC));

		/* If there is no URI the goto dialog will pop up so there is
		 * no need to call setup_session(). */
		if (!uri) return 1;

#ifdef CONFIG_BOOKMARKS
	} else if (!uri && get_opt_bool("ui.sessions.auto_restore")) {
		unsigned char *folder; /* UTF-8 */

		folder = get_auto_save_bookmark_foldername_utf8();
		if (folder) {
			open_bookmark_folder(ses, folder);
			mem_free(folder);
		}
		return 1;
#endif
	}

	/* If there's a URI to load we have to call */
	return 0;
}

/** First load the current URI of the base session. In most cases it will just
 * be fetched from the cache so that the new tab will not appear ``empty' while
 * loading the real URI or showing the goto URL dialog. */
static void
setup_session(struct session *ses, struct uri *uri, struct session *base)
{
	if (base && have_location(base)) {
		ses_load(ses, get_uri_reference(cur_loc(base)->vs.uri),
		         NULL, NULL, CACHE_MODE_ALWAYS, TASK_FORWARD);
		if (ses->doc_view && ses->doc_view->vs
		    && base->doc_view && base->doc_view->vs) {
			struct view_state *vs = ses->doc_view->vs;

			destroy_vs(vs, 1);
			copy_vs(vs, base->doc_view->vs);

			ses->doc_view->vs = vs;
		}
	}

	if (uri) {
		goto_uri(ses, uri);

	} else if (!goto_url_home(ses)) {
		if (get_opt_bool("ui.startup_goto_dialog")) {
			dialog_goto_url_open(ses);
		}
	}
}

/** @relates session  */
struct session *
init_session(struct session *base_session, struct terminal *term,
	     struct uri *uri, int in_background)
{
	struct session *ses = mem_calloc(1, sizeof(*ses));

	if (!ses) return NULL;

	ses->tab = init_tab(term, ses, tabwin_func);
	if (!ses->tab) {
		mem_free(ses);
		return NULL;
	}

	create_history(&ses->history);
	init_list(ses->scrn_frames);
	init_list(ses->more_files);
	init_list(ses->type_queries);
	ses->task.type = TASK_NONE;
	ses->display_timer = TIMER_ID_UNDEF;

#ifdef CONFIG_LEDS
	init_led_panel(&ses->status.leds);
	ses->status.ssl_led = register_led(ses, 0);
	ses->status.insert_mode_led = register_led(ses, 1);
	ses->status.ecmascript_led = register_led(ses, 2);
	ses->status.popup_led = register_led(ses, 3);
#endif
	ses->status.force_show_status_bar = -1;
	ses->status.force_show_title_bar = -1;

	add_to_list(sessions, ses);

	/* Update the status -- most importantly the info about whether to the
	 * show the title, tab and status bar -- _before_ loading the URI so
	 * the document cache is not filled with useless documents if the
	 * content is already cached.
	 *
	 * For big document it also reduces memory usage quite a bit because
	 * (atleast that is my interpretation --jonas) the old document will
	 * have a chance to be released before rendering a new one. A few
	 * numbers when opening a new tab while viewing debians package list
	 * for unstable:
	 *
	 * 9307 jonas     15   0 34252  30m 5088 S  0.0 12.2   0:03.63 elinks-old
	 * 9305 jonas     15   0 17060  13m 5088 S  0.0  5.5   0:02.07 elinks-new
	 */
	update_status();

	/* Check if the newly inserted session is the only in the list and do
	 * the special setup for the first session, */
	if (!list_is_singleton(sessions) || !setup_first_session(ses, uri)) {
		setup_session(ses, uri, base_session);
	}

	if (!in_background)
		switch_to_tab(term, get_tab_number(ses->tab), -1);

	if (!term->main_menu) activate_bfu_technology(ses, -1);
	return ses;
}

static void
init_remote_session(struct session *ses, enum remote_session_flags *remote_ptr,
		    struct uri *uri)
{
	enum remote_session_flags remote = *remote_ptr;

	if (remote & SES_REMOTE_CURRENT_TAB) {
		goto_uri(ses, uri);
		/* Mask out the current tab flag */
		*remote_ptr = remote & ~SES_REMOTE_CURRENT_TAB;

		/* Remote session was masked out. Open all following URIs in
		 * new tabs, */
		if (!*remote_ptr)
			*remote_ptr = SES_REMOTE_NEW_TAB;

	} else if (remote & SES_REMOTE_NEW_TAB) {
		/* FIXME: This is not perfect. Doing multiple -remote
		 * with no URLs will make the goto dialogs
		 * inaccessible. Maybe we should not support this kind
		 * of thing or make the window focus detecting code
		 * more intelligent. --jonas */
		open_uri_in_new_tab(ses, uri, 0, 1);

		if (remote & SES_REMOTE_PROMPT_URL) {
			dialog_goto_url_open(ses);
		}

	} else if (remote & SES_REMOTE_NEW_WINDOW) {
		/* FIXME: It is quite rude because we just take the first
		 * possibility and should maybe make it possible to specify
		 * new-screen etc via -remote "openURL(..., new-*)" --jonas */
		if (!can_open_in_new(ses->tab->term))
			return;

		open_uri_in_new_window(ses, uri, NULL,
				       ses->tab->term->environment,
				       CACHE_MODE_NORMAL, TASK_NONE);

	} else if (remote & SES_REMOTE_ADD_BOOKMARK) {
#ifdef CONFIG_BOOKMARKS
		int uri_cp;

		if (!uri) return;
		/** @todo Bug 1066: What is the encoding of struri()?
		 * This code currently assumes the system charset.
		 * It might be best to keep URIs in plain ASCII and
		 * then have a function that reversibly converts them
		 * to IRIs for display in a given encoding.  */
		uri_cp = get_cp_index("System");
		add_bookmark_cp(NULL, 1, uri_cp, struri(uri), struri(uri));
#endif

	} else if (remote & SES_REMOTE_INFO_BOX) {
		unsigned char *text;

		if (!uri) return;

		text = memacpy(uri->data, uri->datalen);
		if (!text) return;

		info_box(ses->tab->term, MSGBOX_FREE_TEXT,
			 N_("Error"), ALIGN_CENTER,
			 text);

	} else if (remote & SES_REMOTE_PROMPT_URL) {
		dialog_goto_url_open(ses);
	}
}



struct string *
encode_session_info(struct string *info,
		    LIST_OF(struct string_list_item) *url_list)
{
	struct string_list_item *url;

	if (!init_string(info)) return NULL;

	foreach (url, *url_list) {
		struct string *str = &url->string;

		add_bytes_to_string(info, str->source, str->length + 1);
	}

	return info;
}

/** Older elinks versions (up to and including 0.9.1) sends no magic variable and if
 * this is detected we fallback to the old session info format. For this format
 * the magic member of terminal_info hold the length of the URI string. The
 * old format is handled by the default label in the switch.
 *
 * The new session info format supports extraction of multiple URIS from the
 * terminal_info data member. The magic variable controls how to interpret
 * the fields:
 *
 *    -	INTERLINK_NORMAL_MAGIC means use the terminal_info session_info
 *	variable as an id for a saved session.
 *
 *    -	INTERLINK_REMOTE_MAGIC means use the terminal_info session_info
 *	variable as the remote session flags. */
int
decode_session_info(struct terminal *term, struct terminal_info *info)
{
	int len = info->length;
	struct session *base_session = NULL;
	enum remote_session_flags remote = 0;
	unsigned char *str;

	switch (info->magic) {
	case INTERLINK_NORMAL_MAGIC:
		/* Lookup if there are any saved sessions that should be
		 * resumed using the session_info as an id. The id is derived
		 * from the value of -base-session command line option in the
		 * connecting instance.
		 *
		 * At the moment it is only used when opening instances in new
		 * window to figure out how to initialize it when the new
		 * instance connects to the master.
		 *
		 * We don't need to handle it for the old format since new
		 * instances connecting this way will always be of the same
		 * origin as the master. */
		if (init_saved_session(term, info->session_info))
			return 1;
		break;

	case INTERLINK_REMOTE_MAGIC:
		/* This could mean some fatal bug but I am unsure if we can
		 * actually assert it since people could pour all kinds of crap
		 * down the socket. */
		if (!info->session_info) {
			INTERNAL("Remote magic with no remote flags");
			return 0;
		}

		remote = info->session_info;

		/* If processing session info from a -remote instance we want
		 * to hook up with the master so we can handle request for
		 * stuff in current tab. */
		base_session = get_master_session();
		if (!base_session) return 0;

		break;

	default:
		/* The old format calculates the session_info and magic members
		 * as part of the data that should be decoded so we have to
		 * substract it to get the size of the actual URI data. */
		len -= sizeof(info->session_info) + sizeof(info->magic);

		/* Extract URI containing @magic bytes */
		if (info->magic <= 0 || info->magic > len)
			len = 0;
		else
			len = info->magic;
	}

	if (len <= 0) {
		if (!remote)
			return !!init_session(base_session, term, NULL, 0);

		/* Even though there are no URIs we still have to
		 * handle remote stuff. */
		init_remote_session(base_session, &remote, NULL);
		return 0;
	}

	str = info->data;

	/* Extract multiple (possible) NUL terminated URIs */
	while (len > 0) {
		unsigned char *end = memchr(str, 0, len);
		int urilength = end ? end - str : len;
		struct uri *uri = NULL;
		unsigned char *uristring = memacpy(str, urilength);

		if (uristring) {
			uri = get_hooked_uri(uristring, base_session, term->cwd);
			mem_free(uristring);
		}

		len -= urilength + 1;
		str += urilength + 1;

		if (remote) {
			if (!uri) continue;

			init_remote_session(base_session, &remote, uri);

		} else {
			int backgrounded = !list_empty(term->windows);
			int bad_url = !uri;
			struct session *ses;

			if (!uri)
				uri = get_uri("about:blank", 0);

			ses = init_session(base_session, term, uri, backgrounded);
			if (!ses) {
				/* End loop if initialization fails */
				len = 0;
			} else if (bad_url) {
				print_error_dialog(ses, connection_state(S_BAD_URL),
						   NULL, PRI_MAIN);
			}

		}

		if (uri) done_uri(uri);
	}

	/* If we only initialized remote sessions or didn't manage to add any
	 * new tabs return zero so the terminal will be destroyed ASAP. */
	return remote ? 0 : !list_empty(term->windows);
}


void
abort_loading(struct session *ses, int interrupt)
{
	if (have_location(ses)) {
		struct location *loc = cur_loc(ses);

		cancel_download(&loc->download, interrupt);
		abort_files_load(ses, interrupt);
	}
	abort_preloading(ses, interrupt);
}

static void
destroy_session(struct session *ses)
{
	struct document_view *doc_view;

	assert(ses);
	if_assert_failed return;

	destroy_downloads(ses);
	abort_loading(ses, 0);
	free_files(ses);
	if (ses->doc_view) {
		detach_formatted(ses->doc_view);
		mem_free(ses->doc_view);
	}

	foreach (doc_view, ses->scrn_frames)
		detach_formatted(doc_view);

	free_list(ses->scrn_frames);

	destroy_history(&ses->history);
	set_session_referrer(ses, NULL);

	if (ses->loading_uri) done_uri(ses->loading_uri);

	kill_timer(&ses->display_timer);

	while (!list_empty(ses->type_queries))
		done_type_query(ses->type_queries.next);

	if (ses->download_uri) done_uri(ses->download_uri);
	mem_free_if(ses->search_word);
	mem_free_if(ses->last_search_word);
	mem_free_if(ses->status.last_title);
#ifdef CONFIG_ECMASCRIPT
	mem_free_if(ses->status.window_status);
#endif
	del_from_list(ses);
}

void
reload(struct session *ses, enum cache_mode cache_mode)
{
	abort_loading(ses, 0);

	if (cache_mode == CACHE_MODE_INCREMENT) {
		cache_mode = CACHE_MODE_NEVER;
		if (ses->reloadlevel < CACHE_MODE_NEVER)
			cache_mode = ++ses->reloadlevel;
	} else {
		ses->reloadlevel = cache_mode;
	}

	if (have_location(ses)) {
		struct location *loc = cur_loc(ses);
		struct file_to_load *ftl;

#ifdef CONFIG_ECMASCRIPT
		loc->vs.ecmascript_fragile = 1;
#endif

		/* FIXME: When reloading use loading_callback and set up a
		 * session task so that the reloading will work even when the
		 * reloaded document contains redirects. This is needed atleast
		 * when reloading HTTP auth document after the user has entered
		 * credentials. */
		loc->download.data = ses;
		loc->download.callback = (download_callback_T *) doc_loading_callback;

		load_uri(loc->vs.uri, ses->referrer, &loc->download, PRI_MAIN, cache_mode, -1);

		foreach (ftl, ses->more_files) {
			if (file_to_load_is_active(ftl))
				continue;

			ftl->download.data = ftl;
			ftl->download.callback = (download_callback_T *) file_loading_callback;

			load_additional_file(ftl, cache_mode);
		}
	}
}


struct frame *
ses_find_frame(struct session *ses, unsigned char *name)
{
	struct location *loc = cur_loc(ses);
	struct frame *frame;

	assertm(have_location(ses), "ses_request_frame: no location yet");
	if_assert_failed return NULL;

	foreachback (frame, loc->frames)
		if (!c_strcasecmp(frame->name, name))
			return frame;

	return NULL;
}

void
set_session_referrer(struct session *ses, struct uri *referrer)
{
	if (ses->referrer) done_uri(ses->referrer);
	ses->referrer = referrer ? get_uri_reference(referrer) : NULL;
}

static void
tabwin_func(struct window *tab, struct term_event *ev)
{
	struct session *ses = tab->data;

	switch (ev->ev) {
		case EVENT_ABORT:
			if (ses) destroy_session(ses);
			if (!list_empty(sessions)) update_status();
			break;
		case EVENT_INIT:
		case EVENT_RESIZE:
			tab->resize = 1;
			/* fall-through */
		case EVENT_REDRAW:
			if (!ses || ses->tab != get_current_tab(ses->tab->term))
				break;

			draw_formatted(ses, tab->resize);
			if (tab->resize) {
				load_frames(ses, ses->doc_view);
				process_file_requests(ses);
				tab->resize = 0;
			}
			print_screen_status(ses);
			break;
		case EVENT_KBD:
		case EVENT_MOUSE:
			if (!ses) break;
			/* This check is related to bug 296 */
			assert(ses->tab == get_current_tab(ses->tab->term));
			send_event(ses, ev);
			break;
	}
}

/**
 * Gets the url being viewed by this session. Writes it into @a str.
 * A maximum of @a str_size bytes (including null) will be written.
 * @relates session
 */
unsigned char *
get_current_url(struct session *ses, unsigned char *str, size_t str_size)
{
	struct uri *uri;
	int length;

	assert(str && str_size > 0);

	uri = have_location(ses) ? cur_loc(ses)->vs.uri : ses->loading_uri;

	/* Not looking or loading anything */
	if (!uri) return NULL;

	/* Ensure that the url size is not greater than str_size.
	 * We can't just happily strncpy(str, here, str_size)
	 * because we have to stop at POST_CHAR, not only at NULL. */
	length = int_min(get_real_uri_length(uri), str_size - 1);

	return safe_strncpy(str, struri(uri), length + 1);
}

/**
 * Gets the title of the page being viewed by this session. Writes it into
 * @a str.  A maximum of @a str_size bytes (including null) will be written.
 * @relates session
 */
unsigned char *
get_current_title(struct session *ses, unsigned char *str, size_t str_size)
{
	struct document_view *doc_view = current_frame(ses);

	assert(str && str_size > 0);

	/* Ensure that the title is defined */
	/* TODO: Try globhist --jonas */
	if (doc_view && doc_view->document->title)
		return safe_strncpy(str, doc_view->document->title, str_size);

	return NULL;
}

/**
 * Gets the url of the link currently selected. Writes it into @a str.
 * A maximum of @a str_size bytes (including null) will be written.
 * @relates session
 */
unsigned char *
get_current_link_url(struct session *ses, unsigned char *str, size_t str_size)
{
	struct link *link = get_current_session_link(ses);

	assert(str && str_size > 0);

	if (!link) return NULL;

	return safe_strncpy(str, link->where ? link->where : link->where_img, str_size);
}

/** get_current_link_name: returns the name of the current link
 * (the text between @<A> and @</A>), @a str is a preallocated string,
 * @a str_size includes the null char.
 * @relates session */
unsigned char *
get_current_link_name(struct session *ses, unsigned char *str, size_t str_size)
{
	struct link *link = get_current_session_link(ses);
	unsigned char *where, *name = NULL;

	assert(str && str_size > 0);

	if (!link) return NULL;

	where = link->where ? link->where : link->where_img;
#ifdef CONFIG_GLOBHIST
	{
		struct global_history_item *item;

		item = get_global_history_item(where);
		if (item) name = item->title;
	}
#endif
	if (!name) name = get_link_name(link);
	if (!name) name = where;

	return safe_strncpy(str, name, str_size);
}

struct link *
get_current_link_in_view(struct document_view *doc_view)
{
	struct link *link = get_current_link(doc_view);

	return link && !link_is_form(link) ? link : NULL;
}

/** @relates session */
struct link *
get_current_session_link(struct session *ses)
{
	return get_current_link_in_view(current_frame(ses));
}

/** @relates session */
int
eat_kbd_repeat_count(struct session *ses)
{
	int count = ses->kbdprefix.repeat_count;

	ses->kbdprefix.repeat_count = 0;

	/* Clear status bar when prefix is eaten (bug 930) */
	print_screen_status(ses);
	return count;
}
