/** Downloads managment
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_CYGWIN_H
#include <sys/cygwin.h>
#endif
#include <sys/types.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#include <sys/stat.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <utime.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "cache/cache.h"
#include "config/options.h"
#include "dialogs/document.h"
#include "dialogs/download.h"
#include "dialogs/menu.h"
#include "intl/libintl.h"
#include "main/object.h"
#include "main/select.h"
#include "mime/mime.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/state.h"
#include "osdep/osdep.h"
#include "protocol/bittorrent/dialogs.h"
#include "protocol/date.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/download.h"
#include "session/history.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/file.h"
#include "util/hash.h"
#include "util/lists.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/time.h"

/* TODO: tp_*() should be in separate file, I guess? --pasky */

INIT_LIST_OF(struct file_download, downloads);

static struct hash *uri_tempfiles;

void
clear_uri_tempfiles(void)
{
	struct hash_item *item;
	int i;

	if (!uri_tempfiles) {
		return;
	}

	foreach_hash_item (item, *uri_tempfiles, i) {
		mem_free_set(&item->value, NULL);
		mem_free_set(&item->key, NULL);
	}
	free_hash(&uri_tempfiles);
}

static char *
check_url_tempfiles(const char *url)
{
	struct hash_item *item;

	if (!uri_tempfiles || !url) {
		return NULL;
	}
	item = get_hash_item(uri_tempfiles, url, strlen(url));

	if (!item || !item->value) {
		return NULL;
	}

	return stracpy(item->value);
}

static void
set_uri_tempfile(const char *url, const char *value)
{
	if (!uri_tempfiles) {
		uri_tempfiles = init_hash8();
	}

	if (uri_tempfiles) {
		size_t len = strlen(url);
		char *copy = memacpy(url, len);

		if (copy) {
			add_hash_item(uri_tempfiles, copy, len, stracpy(value));
		}
	}
}

int
download_is_progressing(struct download *download)
{
	return download
	    && is_in_state(download->state, S_TRANS)
	    && has_progress(download->progress);
}

int
are_there_downloads(void)
{
	struct file_download *file_download;

	foreach (file_download, downloads)
		if (!file_download->external_handler)
			return 1;

	return 0;
}


static void download_data(struct download *download, struct file_download *file_download);

/*! @note If this fails, the caller is responsible of freeing @a file
 * and closing @a fd.  */
struct file_download *
init_file_download(struct uri *uri, struct session *ses, char *file, int fd)
{
	struct file_download *file_download;

	file_download = (struct file_download *)mem_calloc(1, sizeof(*file_download));
	if (!file_download) return NULL;

	/* Actually we could allow fragments in the URI and just change all the
	 * places that compares and shows the URI, but for now it is much
	 * easier this way. */
	file_download->uri = get_composed_uri(uri, URI_BASE);
	if (!file_download->uri) {
		mem_free(file_download);
		return NULL;
	}

	init_download_display(file_download);

	file_download->file = file;
	file_download->handle = fd;

	file_download->download.callback = (download_callback_T *) download_data;
	file_download->download.data = file_download;
	file_download->ses = ses;
	/* The tab may be closed, but we will still want to ie. open the
	 * handler on that terminal. */
	file_download->term = ses->tab->term;

	object_nolock(file_download, "file_download"); /* Debugging purpose. */
	add_to_list(downloads, file_download);

	return file_download;
}


void
abort_download(struct file_download *file_download)
{
#if 0
	/* When hacking to cleanup the download code, remove lots of duplicated
	 * code and implement stuff from bug 435 we should reintroduce this
	 * assertion. Currently it will trigger often and shows that the
	 * download dialog code potentially could access free()d memory. */
	assert(!is_object_used(file_download));
#endif
	done_download_display(file_download);

	if (file_download->ses)
		check_questions_queue(file_download->ses);

	if (file_download->dlg_data) {
		if (file_download->dlg_data->dlg && file_download->dlg_data->dlg->refresh) {
			kill_timer(&file_download->dlg_data->dlg->refresh->timer);
		}
		cancel_dialog(file_download->dlg_data, NULL);
	}
	cancel_download(&file_download->download, file_download->stop);
	if (file_download->uri) done_uri(file_download->uri);

	if (file_download->handle != -1) {
		prealloc_truncate(file_download->handle, file_download->seek);
		close(file_download->handle);
	}

	mem_free_if(file_download->external_handler);
	if (file_download->file) {
		if (file_download->delete_) unlink(file_download->file);
		mem_free(file_download->file);
	}
	mem_free_if(file_download->inpext);
	mem_free_if(file_download->outext);
	del_from_list(file_download);
	mem_free(file_download);
}


static void
kill_downloads_to_file(char *file)
{
	struct file_download *file_download;

	foreach (file_download, downloads) {
		if (strcmp(file_download->file, file))
			continue;

		file_download = file_download->prev;
		abort_download(file_download->next);
	}
}


void
abort_all_downloads(void)
{
	while (!list_empty(downloads))
		abort_download((struct file_download *)downloads.next);
}


void
destroy_downloads(struct session *ses)
{
	struct file_download *file_download, *next;
	struct session *s;

	/* We are supposed to blat all downloads to external handlers belonging
	 * to @ses, but we will refuse to do so if there is another session
	 * bound to this terminal. That looks like the reasonable thing to do,
	 * fulfilling the principle of least astonishment. */
	foreach (s, sessions) {
		if (s == ses || s->tab->term != ses->tab->term)
			continue;

		foreach (file_download, downloads) {
			if (file_download->ses != ses)
				continue;

			file_download->ses = s;
		}

		return;
	}

	foreachsafe (file_download, next, downloads) {
		if (file_download->ses != ses)
			continue;

		if (!file_download->external_handler) {
			file_download->ses = NULL;
			continue;
		}

		abort_download(file_download);
	}
}

void
detach_downloads_from_terminal(struct terminal *term)
{
	struct file_download *file_download, *next;

	assert(term != NULL);
	if_assert_failed return;

	foreachsafe (file_download, next, downloads) {
		if (file_download->term != term)
			continue;

		if (!file_download->external_handler) {
			file_download->term = NULL;
			if (file_download->ses
			    && file_download->ses->tab->term == term)
				file_download->ses = NULL;
			continue;
		}

		abort_download(file_download);
	}
}

static void
download_error_dialog(struct file_download *file_download, int saved_errno)
{
	char *emsg = (char *) strerror(saved_errno);
	struct session *ses = file_download->ses;
	struct terminal *term = file_download->term;

	if (!ses) return;

	info_box(term, MSGBOX_FREE_TEXT,
		 N_("Download error"), ALIGN_CENTER,
		 msg_text(term, N_("Could not create file '%s':\n%s"),
			  file_download->file, emsg));
}

static int
write_cache_entry_to_file(struct cache_entry *cached, struct file_download *file_download)
{
	struct fragment *frag;

	if (file_download->download.progress && file_download->download.progress->seek) {
		file_download->seek = file_download->download.progress->seek;
		file_download->download.progress->seek = 0;
		/* This is exclusive with the prealloc, thus we can perform
		 * this in front of that thing safely. */
		if (lseek(file_download->handle, file_download->seek, SEEK_SET) < 0) {
			download_error_dialog(file_download, errno);
			return 0;
		}
	}

	foreach (frag, cached->frag) {
		off_t remain = file_download->seek - frag->offset;
		int *h = &file_download->handle;
		ssize_t w;

		if (remain < 0 || frag->length <= remain)
			continue;

#ifdef USE_OPEN_PREALLOC
		if (!file_download->seek
		    && (!file_download->download.progress
			|| file_download->download.progress->size > 0)) {
			close(*h);
			*h = open_prealloc(file_download->file,
					   O_CREAT|O_WRONLY|O_TRUNC,
					   0666,
					   file_download->download.progress
					   ? file_download->download.progress->size
					   : cached->length);
			if (*h == -1) {
				download_error_dialog(file_download, errno);
				return 0;
			}
			set_bin(*h);
		}
#endif

		w = safe_write(*h, frag->data + remain, frag->length - remain);
		if (w == -1) {
			download_error_dialog(file_download, errno);
			return 0;
		}

		file_download->seek += w;
	}

	return 1;
}

static void
abort_download_and_beep(struct file_download *file_download, struct terminal *term)
{
	if (term && get_opt_int("document.download.notify_bell",
	                        file_download->ses)
		    + file_download->notify >= 2) {
		beep_terminal(term);
	}

	abort_download(file_download);
}

struct exec_mailcap {
	struct session *ses;
	char *command;
	char *file;
};

static void
do_follow_url_mailcap(struct session *ses, struct uri *uri)
{
	if (!uri) {
		print_error_dialog(ses, connection_state(S_BAD_URL), uri, PRI_CANCEL);
		return;
	}

	ses->reloadlevel = CACHE_MODE_NORMAL;

	if (ses->task.type == TASK_FORWARD) {
		if (compare_uri(ses->loading_uri, uri, 0)) {
			/* We're already loading the URL. */
			return;
		}
	}

	abort_loading(ses, 0);

	ses_goto(ses, uri, NULL, NULL, CACHE_MODE_NORMAL, TASK_FORWARD, 0);
}

static void
exec_mailcap_command(void *data)
{
	struct exec_mailcap *exec_mailcap = (struct exec_mailcap *)data;

	if (exec_mailcap) {
		if (exec_mailcap->command) {
			struct string string;

			if (init_string(&string)) {
				static char mailcap_elmailcap[] = "mailcap:elmailcap";
				struct uri *ref = get_uri(mailcap_elmailcap, URI_NONE);
				struct uri *uri;
				struct session *ses = exec_mailcap->ses;

				add_to_string(&string, "mailcap:");
				add_to_string(&string, exec_mailcap->command);
				if (exec_mailcap->file) {
					add_to_string(&string, " && /bin/rm -f ");
					add_to_string(&string, exec_mailcap->file);
				}

				uri = get_uri(string.source, URI_NONE);
				done_string(&string);
				set_session_referrer(ses, ref);
				if (ref) done_uri(ref);

				do_follow_url_mailcap(ses, uri);
				if (uri) done_uri(uri);
			}
			mem_free(exec_mailcap->command);
		}
		mem_free_if(exec_mailcap->file);
		mem_free(exec_mailcap);
	}
}

static void
exec_later(struct session *ses, char *handler, char *file)
{
	struct exec_mailcap *exec_mailcap = (struct exec_mailcap *)mem_calloc(1, sizeof(*exec_mailcap));

	if (exec_mailcap) {
		exec_mailcap->ses = ses;
		exec_mailcap->command = null_or_stracpy(handler);
		exec_mailcap->file = null_or_stracpy(file);
		register_bottom_half(exec_mailcap_command, exec_mailcap);
	}
}

static void
exec_dgi_command(void *data)
{
	struct exec_dgi *exec_dgi = (struct exec_dgi *)data;

	if (exec_dgi) {
		if (exec_dgi->command) {
			struct string string;

			if (init_string(&string)) {
				static char dgi_dgi[] = "dgi://";
				struct uri *ref = get_uri(dgi_dgi, URI_NONE);
				struct uri *uri;
				struct session *ses = exec_dgi->ses;

				add_to_string(&string, "dgi:///dgi?command=");
				add_to_string(&string, exec_dgi->command);
				add_to_string(&string, "&filename=");
				if (exec_dgi->file) {
					add_to_string(&string, exec_dgi->file);
				}
				add_to_string(&string, "&inpext=");
				if (exec_dgi->inpext) {
					add_to_string(&string, exec_dgi->inpext);
				}
				add_to_string(&string, "&outext=");
				if (exec_dgi->outext) {
					add_to_string(&string, exec_dgi->outext);
				}
				if (exec_dgi->del) {
					add_to_string(&string, "&delete=1");
				}
				uri = get_uri(string.source, URI_BASE_FRAGMENT);
				done_string(&string);
				set_session_referrer(ses, ref);
				if (ref) done_uri(ref);

				do_follow_url_mailcap(ses, uri);
				if (uri) done_uri(uri);
			}
			mem_free(exec_dgi->command);
		}
		mem_free_if(exec_dgi->file);
		mem_free_if(exec_dgi->inpext);
		mem_free_if(exec_dgi->outext);
		mem_free(exec_dgi);
	}
}

static void
exec_later_dgi(struct session *ses, char *handler, char *file, char *inpext, char *outext, int del)
{
	struct exec_dgi *exec_dgi = (struct exec_dgi *)mem_calloc(1, sizeof(*exec_dgi));

	if (exec_dgi) {
		exec_dgi->ses = ses;
		exec_dgi->command = null_or_stracpy(handler);
		exec_dgi->file = null_or_stracpy(file);
		exec_dgi->inpext = null_or_stracpy(inpext);
		exec_dgi->outext = null_or_stracpy(outext);
		exec_dgi->del = del;
		register_bottom_half(exec_dgi_command, exec_dgi);
	}
}

static void
download_data_store(struct download *download, struct file_download *file_download)
{
	struct terminal *term = file_download->term;

	assert_terminal_ptr_not_dangling(term);
	if_assert_failed term = file_download->term = NULL;

	if (is_in_progress_state(download->state)) {
		return;
	}

	/* If the original terminal of the download has been closed,
	 * display any messages in the default terminal instead.  */
	if (term == NULL)
		term = get_default_terminal(); /* may be NULL too */

	if (!is_in_state(download->state, S_OK)) {
		char *url = get_uri_string(file_download->uri, URI_PUBLIC);
		struct connection_state state = download->state;

		/* abort_download_and_beep allows term==NULL.  */
		abort_download_and_beep(file_download, term);

		if (!url) return;

		if (term) {
			info_box(term, MSGBOX_FREE_TEXT,
				 N_("Download error"), ALIGN_CENTER,
				 msg_text(term, N_("Error downloading %s:\n\n%s"),
					  url, get_state_message(state, term)));
		}
		mem_free(url);
		return;
	}

	if (file_download->external_handler) {
		if (term == NULL) {
			/* There is no terminal in which to run the handler.
			 * Abort the download.  file_download->delete_ should
			 * be 1 here so that the following call also deletes
			 * the temporary file.  */ 
			abort_download(file_download);
			return;
		}

		prealloc_truncate(file_download->handle, file_download->seek);
		close(file_download->handle);
		file_download->handle = -1;
		if (file_download->copiousoutput) {
			exec_later(file_download->ses,
				   file_download->external_handler, file_download->file);
			/* Temporary file is deleted by the mailcap_protocol_handler */
			file_download->delete_ = 0;
		} else if (file_download->dgi) {
			exec_later_dgi(file_download->ses,
				   file_download->external_handler, file_download->file,
				   file_download->inpext, file_download->outext, 1);
			/* Temporary file is deleted by the dgi_protocol_handler */
			file_download->delete_ = 0;
		} else {
			if (get_opt_bool("ui.sessions.postpone_unlink", NULL)) {
				char *url = get_uri_string(file_download->uri, URI_PUBLIC);

				if (url) {
					set_uri_tempfile(url, file_download->file);
					mem_free(url);
				}
			}
			exec_on_terminal(term, file_download->external_handler,
					 file_download->file,
					 file_download->block ? TERM_EXEC_FG :
					 TERM_EXEC_BG);
		}
		file_download->delete_ = 0;
		abort_download_and_beep(file_download, term);
		return;
	}

	if (file_download->notify && term) {
		char *url = get_uri_string(file_download->uri, URI_PUBLIC);

		/* This is apparently a little racy. Deleting the box item will
		 * update the download browser _after_ the notification dialog
		 * has been drawn whereby it will be hidden. This should make
		 * the download browser update before launcing any
		 * notification. */
		done_download_display(file_download);

		if (url) {
			info_box(term, MSGBOX_FREE_TEXT,
				 N_("Download"), ALIGN_CENTER,
				 msg_text(term, N_("Download complete:\n%s"), url));
			mem_free(url);
		}
	}

	if (file_download->remotetime
	    && get_opt_bool("document.download.set_original_time",
	                    file_download->ses)) {
		struct utimbuf foo;

		foo.actime = foo.modtime = file_download->remotetime;
		utime(file_download->file, &foo);
	}

	/* abort_download_and_beep allows term==NULL.  */
	abort_download_and_beep(file_download, term);
}

static void
download_data(struct download *download, struct file_download *file_download)
{
	struct cache_entry *cached = download->cached;

	if (!cached || is_in_queued_state(download->state)) {
		download_data_store(download, file_download);
		return;
	}

	if (cached->last_modified)
		file_download->remotetime = parse_date(&cached->last_modified, NULL, 0, 1);

	if (cached->redirect && file_download->redirect_cnt++ < MAX_REDIRECTS) {
		cancel_download(&file_download->download, 0);

		assertm(compare_uri(cached->uri, file_download->uri, 0),
			"Redirecting using bad base URI");

		done_uri(file_download->uri);

		file_download->uri = get_uri_reference(cached->redirect);
		file_download->download.state = connection_state(S_WAIT_REDIR);

		load_uri(file_download->uri, cached->uri, &file_download->download,
			 PRI_DOWNLOAD, CACHE_MODE_NORMAL,
			 download->progress ? download->progress->start : 0);

		return;
	}

	if (!write_cache_entry_to_file(cached, file_download)) {
		detach_connection(download, file_download->seek);
		abort_download(file_download);
		return;
	}

	detach_connection(download, file_download->seek);
	download_data_store(download, file_download);
}

/** Type of the callback function that will be called when the user
 * answers the question posed by lookup_unique_name().
 *
 * @param term
 * The terminal on which the callback should display any windows.
 * Comes directly from the @a term argument of lookup_unique_name().
 *
 * @param file
 * The name of the local file to which the data should be downloaded,
 * or NULL if the download should not begin.  The callback is
 * responsible of doing mem_free(@a file).
 *
 * @param data
 * A pointer to any data that the callback cares about.
 * Comes directly from the @a data argument of lookup_unique_name().
 *
 * @param flags
 * The same as the @a flags argument of create_download_file(),
 * except the ::DOWNLOAD_RESUME_SELECTED bit will be changed to match
 * what the user chose.
 *
 * @relates lun_hop */
typedef void lun_callback_T(struct terminal *term, char *file,
			    void *data, download_flags_T flags);

/** The user is being asked what to do when the local file for
 * the download already exists.  This structure is allocated by
 * lookup_unique_name() and freed by each lun_* function:
 * lun_alternate(), lun_cancel(), lun_overwrite(), and lun_resume().  */
struct lun_hop {
	/** The terminal in which ELinks is asking the question.
	 * This gets passed to #callback.  */
	struct terminal *term;

	/** The name of the local file into which the data was
	 * originally going to be downloaded, but which already
	 * exists.  In this string, "~" has already been expanded
	 * to the home directory.  The string must be freed with
	 * mem_free().  */
	char *ofile;

	/** An alternative file name that the user may choose instead
	 * of #ofile.  The string must be freed with mem_free().  */
	char *file;

	/** This function will be called when the user answers.  */
	lun_callback_T *callback;

	/** A pointer to be passed to #callback.  */
	void *data;

	/** Saved flags to be passed to #callback.
	 * If the user chooses to resume, then lun_resume() sets
	 * ::DOWNLOAD_RESUME_SELECTED when it calls #callback.
	 *
	 * @invariant The ::DOWNLOAD_RESUME_SELECTED bit should be
	 * clear here because otherwise there would have been no
	 * reason to ask the user and initialize this structure.  */
	download_flags_T flags;
};

/** Data saved by common_download() for the common_download_do()
 * callback.  */
struct cmdw_hop {
	struct session *ses;

	/** The URI from which the data will be downloaded.  */
	struct uri *download_uri;

	/** The name of the local file to which the data will be
	 * downloaded.  This is initially NULL, but its address is
	 * given to create_download_file(), which arranges for the
	 * pointer to be set before common_download_do() is called.
	 * The string must be freed with mem_free().  */
	char *real_file;
};

/** Data saved by continue_download() for the continue_download_do()
 * callback.  */
struct codw_hop {
	struct type_query *type_query;

	/** The name of the local file to which the data will be
	 * downloaded.  This is initially NULL, but its address is
	 * given to create_download_file(), which arranges for the
	 * pointer to be set before continue_download_do() is called.
	 * The string must be freed with mem_free().  */
	char *real_file;

	char *file;
};

/** Data saved by create_download_file() for the create_download_file_do()
 * callback.  */
struct cdf_hop {
	/** Where to save the name of the file that was actually
	 * opened.  One of the arguments of #callback is a file
	 * descriptor for this file.  @c real_file can be NULL if
	 * #callback does not care about the name.  */
	char **real_file;

	/** This function will be called when the file has been opened,
	 * or when it is known that the file will not be opened.  */
	cdf_callback_T *callback;

	/** A pointer to be passed to #callback.  */
	void *data;
};

/** The use chose "Save under the alternative name" when asked where
 * to download a file.
 *
 * lookup_unique_name() passes this function as a ::done_handler_T to
 * msg_box().
 *
 * @relates lun_hop */
static void
lun_alternate(void *lun_hop_)
{
	struct lun_hop *lun_hop = (struct lun_hop *)lun_hop_;

	lun_hop->callback(lun_hop->term, lun_hop->file, lun_hop->data,
			  lun_hop->flags);
	mem_free_if(lun_hop->ofile);
	mem_free(lun_hop);
}

/** The use chose "Cancel" when asked where to download a file.
 *
 * lookup_unique_name() passes this function as a ::done_handler_T to
 * msg_box().
 *
 * @relates lun_hop */
static void
lun_cancel(void *lun_hop_)
{
	struct lun_hop *lun_hop = (struct lun_hop *)lun_hop_;

	lun_hop->callback(lun_hop->term, NULL, lun_hop->data,
			  lun_hop->flags);
	mem_free_if(lun_hop->ofile);
	mem_free_if(lun_hop->file);
	mem_free(lun_hop);
}

/** The use chose "Overwrite the original file" when asked where to
 * download a file.
 *
 * lookup_unique_name() passes this function as a ::done_handler_T to
 * msg_box().
 *
 * @relates lun_hop */
static void
lun_overwrite(void *lun_hop_)
{
	struct lun_hop *lun_hop = (struct lun_hop *)lun_hop_;

	lun_hop->callback(lun_hop->term, lun_hop->ofile, lun_hop->data,
			  lun_hop->flags);
	mem_free_if(lun_hop->file);
	mem_free(lun_hop);
}

/** The user chose "Resume download of the original file" when asked
 * where to download a file.
 *
 * lookup_unique_name() passes this function as a ::done_handler_T to
 * msg_box().
 *
 * @relates lun_hop */
static void
lun_resume(void *lun_hop_)
{
	struct lun_hop *lun_hop = (struct lun_hop *)lun_hop_;

	lun_hop->callback(lun_hop->term, lun_hop->ofile, lun_hop->data,
			  lun_hop->flags | DOWNLOAD_RESUME_SELECTED);
	mem_free_if(lun_hop->file);
	mem_free(lun_hop);
}


/** If attempting to download to an existing file, perhaps ask
 * the user whether to resume, overwrite, or save elsewhere.
 * This function constructs a struct lun_hop, which will be freed
 * when the user answers the question.
 *
 * @param term
 * The terminal in which this function should show its UI.
 *
 * @param[in] ofile
 * A proposed name for the local file to which the data would be
 * downloaded.  "~" here refers to the home directory.
 * lookup_unique_name() treats this original string as read-only.
 *
 * @param[in] flags
 * Flags controlling how to download the file.
 * ::DOWNLOAD_RESUME_ALLOWED adds a "Resume" button to the dialog.
 * ::DOWNLOAD_RESUME_SELECTED means the user already chose to resume
 * downloading (with ::ACT_MAIN_LINK_DOWNLOAD_RESUME), before ELinks
 * even asked for the file name; thus don't ask whether to overwrite.
 * Other flags, such as ::DOWNLOAD_EXTERNAL, have no effect at this
 * level but they get passed to @a callback.
 *
 * @param callback
 * Will be called when the user answers, or right away if the question
 * need not or cannot be asked.
 *
 * @param data
 * A pointer to be passed to @a callback.
 *
 * @relates lun_hop */
static void
lookup_unique_name(struct terminal *term, char *ofile,
		   download_flags_T flags,
		   lun_callback_T *callback, void *data)
{
	/* [gettext_accelerator_context(.lookup_unique_name)] */
	struct lun_hop *lun_hop = NULL;
	char *file = NULL;
	struct dialog_data *dialog_data;
	int overwrite;

	ofile = expand_tilde(ofile);
	if (!ofile) goto error;

	/* Minor code duplication to prevent useless call to get_opt_int()
	 * if possible. --Zas */
	if (flags & DOWNLOAD_RESUME_SELECTED) {
		callback(term, ofile, data, flags);
		return;
	}

	/* !overwrite means always silently overwrite, which may be admitelly
	 * indeed a little confusing ;-) */
	overwrite = get_opt_int("document.download.overwrite", NULL);
	if (!overwrite) {
		/* Nothing special to do... */
		callback(term, ofile, data, flags);
		return;
	}

	/* Check if file is a directory, and use a default name if it's the
	 * case. */
	if (file_is_dir(ofile)) {
		info_box(term, MSGBOX_FREE_TEXT,
			 N_("Download error"), ALIGN_CENTER,
			 msg_text(term, N_("'%s' is a directory."),
				  ofile));
		goto error;
	}

	/* Check if the file already exists (file != ofile). */
	file = (flags & DOWNLOAD_OVERWRITE) ? ofile : get_unique_name(ofile);

	if (!file || overwrite == 1 || file == ofile) {
		/* Still nothing special to do... */
		if (file != ofile) mem_free(ofile);
		callback(term, file, data, flags & ~DOWNLOAD_RESUME_SELECTED);
		return;
	}

	/* overwrite == 2 (ask) and file != ofile (=> original file already
	 * exists) */

	lun_hop = (struct lun_hop *)mem_calloc(1, sizeof(*lun_hop));
	if (!lun_hop) goto error;
	lun_hop->term = term;
	lun_hop->ofile = ofile;
	lun_hop->file = file; /* file != ofile verified above */
	lun_hop->callback = callback;
	lun_hop->data = data;
	lun_hop->flags = flags;

	dialog_data = msg_box(
		term, NULL, MSGBOX_FREE_TEXT,
		N_("File exists"), ALIGN_CENTER,
		msg_text(term, N_("This file already exists:\n"
			"%s\n\n"
			"The alternative filename is:\n"
			"%s"),
			empty_string_or_(lun_hop->ofile),
			empty_string_or_(file)),
		lun_hop, 4,
		MSG_BOX_BUTTON(N_("Sa~ve under the alternative name"), lun_alternate, B_ENTER),
		MSG_BOX_BUTTON(N_("~Overwrite the original file"), lun_overwrite, 0),
		MSG_BOX_BUTTON((flags & DOWNLOAD_RESUME_ALLOWED
				? N_("~Resume download of the original file")
				: NULL),
			       lun_resume, 0),
		MSG_BOX_BUTTON(N_("~Cancel"), lun_cancel, B_ESC));
	if (!dialog_data) goto error;
	return;

error:
	mem_free_if(lun_hop);
	if (file != ofile) mem_free_if(file);
	mem_free_if(ofile);
	callback(term, NULL, data, flags & ~DOWNLOAD_RESUME_SELECTED);
}



/** Now that the final name of the download file has been chosen,
 * open the file and call the ::cdf_callback_T that was originally
 * given to create_download_file().
 *
 * create_download_file() passes this function as a ::lun_callback_T
 * to lookup_unique_name().
 *
 * @relates cdf_hop */
static void
create_download_file_do(struct terminal *term, char *file,
			void *data, download_flags_T flags)
{
	struct cdf_hop *cdf_hop = (struct cdf_hop *)data;
	char *wd;
	int h = -1;
	int saved_errno;
#ifdef NO_FILE_SECURITY
	int sf = 0;
#else
	int sf = !!(flags & DOWNLOAD_EXTERNAL);
#endif

	if (!file) goto finish;

	wd = get_cwd();
	set_cwd(term->cwd);

	/* Create parent directories if needed. */
	mkalldirs(file);

	/* O_APPEND means repositioning at the end of file before each write(),
	 * thus ignoring seek()s and that can hide mysterious bugs. IMHO.
	 * --pasky */
	h = open(file, O_CREAT | O_WRONLY
			| (flags & DOWNLOAD_RESUME_SELECTED ? 0 : O_TRUNC)
			| (sf &&
			   !(flags & DOWNLOAD_RESUME_SELECTED) &&
			   !(flags & DOWNLOAD_OVERWRITE) ? O_EXCL : 0),
		 sf ? 0600 : 0666);
	saved_errno = errno; /* Saved in case of ... --Zas */

	if (wd) {
		set_cwd(wd);
		mem_free(wd);
	}

	if (h == -1) {
		info_box(term, MSGBOX_FREE_TEXT,
			 N_("Download error"), ALIGN_CENTER,
			 msg_text(term, N_("Could not create file '%s':\n%s"),
				  file, strerror(saved_errno)));

		mem_free(file);
		goto finish;

	} else {
		set_bin(h);

		if (!(flags & DOWNLOAD_EXTERNAL)) {
			char *download_dir = get_opt_str("document.download.directory", NULL);
			int i;

			safe_strncpy(download_dir, file, MAX_STR_LEN);

			/* Find the used directory so it's available in history */
			for (i = strlen(download_dir); i >= 0; i--)
				if (dir_sep(download_dir[i]))
					break;
			download_dir[i + 1] = 0;
		}
	}

	if (cdf_hop->real_file)
		*cdf_hop->real_file = file;
	else
		mem_free(file);

finish:
	cdf_hop->callback(term, h, cdf_hop->data, flags);
	mem_free(cdf_hop);
}

/** Create a file to which data can be downloaded.
 * This function constructs a struct cdf_hop that will be freed
 * when @a callback returns.
 *
 * @param term
 * If any dialog boxes are needed, show them in this terminal.
 *
 * @param fi
 * A proposed name for the local file to which the data would be
 * downloaded.  "~" here refers to the home directory.
 * create_download_file() treats this original string as read-only.
 *
 * @param real_file
 * If non-NULL, prepare to save in *@a real_file the name of the local
 * file that was eventually opened.  @a callback must then arrange for
 * this string to be freed with mem_free().
 *
 * @param flags
 * Flags controlling how to download the file.
 * ::DOWNLOAD_RESUME_ALLOWED adds a "Resume" button to the dialog.
 * ::DOWNLOAD_RESUME_SELECTED skips the dialog entirely.
 * ::DOWNLOAD_EXTERNAL causes the file to be created with settings
 * suitable for a temporary file: give only the user herself access to
 * the file (even if the umask is looser), and create the file with
 * @c O_EXCL unless resuming.
 *
 * @param callback
 * This function will be called when the file has been opened,
 * or when it is known that the file will not be opened.
 *
 * @param data
 * A pointer to be passed to @a callback.
 *
 * @relates cdf_hop */
void
create_download_file(struct terminal *term, char *fi,
		     char **real_file,
		     download_flags_T flags,
		     cdf_callback_T *callback, void *data)
{
	struct cdf_hop *cdf_hop = (struct cdf_hop *)mem_calloc(1, sizeof(*cdf_hop));
	char *wd;

	if (!cdf_hop) {
		callback(term, -1, data, flags & ~DOWNLOAD_RESUME_SELECTED);
		return;
	}

	cdf_hop->real_file = real_file;
	cdf_hop->callback = callback;
	cdf_hop->data = data;

	/* FIXME: The wd bussiness is probably useless here? --pasky */
	wd = get_cwd();
	set_cwd(term->cwd);

	/* Also the tilde will be expanded here. */
	lookup_unique_name(term, fi, flags, create_download_file_do, cdf_hop);

	if (wd) {
		set_cwd(wd);
		mem_free(wd);
	}
}


static char *
get_temp_name(struct uri *uri)
{
	char *extension;
	char *nm;

	extension = get_extension_from_uri(uri);
	if (!extension)
		extension = stracpy("");

	nm = tempname(NULL, ELINKS_TEMPNAME_PREFIX, extension);
	mem_free(extension);

	return nm;
}


static char *
subst_file(char *prog, char *file, char *uri)
{
	struct string name;
	/* When there is no %s in the mailcap entry, the handler program reads
	 * data from stdin instead of a file. */
	int input = 1;
	char *replace;
	char *original = (char *)("% ");
	int truncate;
	int tlen = 40;

	if (!init_string(&name)) return NULL;

	while (*prog) {
		int p;

		for (p = 0; prog[p] && prog[p] != '%'; p++);

		add_bytes_to_string(&name, prog, p);
		prog += p;

		if (*prog == '%') {
			prog++;
			truncate = 0;
			if (*prog == 'f' || *prog == ' ' || *prog == '\0')
				replace = file;
			else if (*prog == 'u') {
				replace = uri;
				if (!memcmp(uri, "data:", sizeof("data:") - 1))
					truncate = 1;
			}
			else if (*prog == '%')
				replace = (char *)("%");
			else {
				original[1] = *prog;
				replace = original;
			}

			if (*prog == ' ' || *prog == '\0')
				prog--;

			input = 0;
#if defined(HAVE_CYGWIN_CONV_TO_FULL_WIN32_PATH)
#ifdef MAX_PATH
			char new_path[MAX_PATH];
#else
			char new_path[1024];
#endif

			cygwin_conv_to_full_win32_path(replace, new_path);
			add_to_string(&name, new_path);
#else
			if (! truncate || strlen(replace) <= tlen)
				add_shell_quoted_to_string(&name,
					replace, strlen(replace));
			else {
				add_shell_quoted_to_string(&name,
					replace, tlen);
				add_shell_quoted_to_string(&name,
					"...", sizeof("...") - 1);
			}
#endif
			prog++;
		}
	}

	if (input) {
		struct string s;

		if (init_string(&s)) {
			add_to_string(&s, "/bin/cat ");
			add_shell_quoted_to_string(&s, file, strlen(file));
			add_to_string(&s, " | ");
			add_string_to_string(&s, &name);
			done_string(&name);
			return s.source;
		}
	}
	return name.source;
}



/*! common_download() passes this function as a ::cdf_callback_T to
 * create_download_file().
 *
 * @relates cmdw_hop */
static void
common_download_do(struct terminal *term, int fd, void *data,
		   download_flags_T flags)
{
	struct file_download *file_download;
	struct cmdw_hop *cmdw_hop = (struct cmdw_hop *)data;
	struct uri *download_uri = cmdw_hop->download_uri;
	char *file = cmdw_hop->real_file;
	struct session *ses = cmdw_hop->ses;
	struct stat buf;

	mem_free(cmdw_hop);

	if (!file || fstat(fd, &buf)) goto finish;

	file_download = init_file_download(download_uri, ses, file, fd);
	if (!file_download) goto finish;
	/* If init_file_download succeeds, it takes ownership of file
	 * and fd.  */
	file = NULL;
	fd = -1;

	if (flags & DOWNLOAD_RESUME_SELECTED)
		file_download->seek = buf.st_size;

	display_download(ses->tab->term, file_download, ses);

	load_uri(file_download->uri, ses->referrer, &file_download->download,
		 PRI_DOWNLOAD, CACHE_MODE_NORMAL, file_download->seek);

finish:
	mem_free_if(file);
	if (fd != -1) close(fd);
	done_uri(download_uri);
}

/** Begin or resume downloading from session.download_uri to the
 * @a file specified by the user.
 *
 * This function contains the code shared between start_download() and
 * resume_download().
 *
 * @relates cmdw_hop */
static void
common_download(struct session *ses, char *file,
		download_flags_T flags)
{
	struct cmdw_hop *cmdw_hop;

	if (!ses->download_uri) return;

	cmdw_hop = (struct cmdw_hop *)mem_calloc(1, sizeof(*cmdw_hop));
	if (!cmdw_hop) return;
	cmdw_hop->ses = ses;
	cmdw_hop->download_uri = ses->download_uri;
	ses->download_uri = NULL;

	kill_downloads_to_file(file);

	create_download_file(ses->tab->term, file, &cmdw_hop->real_file,
			     flags, common_download_do, cmdw_hop);
}

/** Begin downloading from session.download_uri to the @a file
 * specified by the user.
 *
 * The ::ACT_MAIN_SAVE_AS, ::ACT_MAIN_SAVE_URL_AS,
 * ::ACT_MAIN_LINK_DOWNLOAD, and ::ACT_MAIN_LINK_DOWNLOAD_IMAGE
 * actions pass this function as the @c std callback to query_file().
 *
 * @relates cmdw_hop */
void
start_download(void *ses, char *file)
{
	common_download((struct session *)ses, file,
			DOWNLOAD_RESUME_ALLOWED);
}


/** Resume downloading from session.download_uri to the @a file
 * specified by the user.
 *
 * The ::ACT_MAIN_LINK_DOWNLOAD_RESUME action passes this function as
 * the @c std callback to query_file().
 *
 * @relates cmdw_hop */
void
resume_download(void *ses, char *file)
{
	common_download((struct session *)ses, file,
			DOWNLOAD_RESUME_ALLOWED | DOWNLOAD_RESUME_SELECTED);
}

/** Resume downloading a file, based on information in struct
 * codw_hop.  This function actually starts a new download from the
 * current end of the file, even though a download from the beginning
 * is already in progress at codw_hop->type_query->download.  The
 * caller will cancel the preexisting download after this function
 * returns.
 *
 * @relates codw_hop */
static void
transform_codw_to_cmdw(struct terminal *term, int fd,
		       struct codw_hop *codw_hop,
		       download_flags_T flags)
{
	struct type_query *type_query = codw_hop->type_query;
	struct cmdw_hop *cmdw_hop = (struct cmdw_hop *)mem_calloc(1, sizeof(*cmdw_hop));

	if (!cmdw_hop) {
		close(fd);
		return;
	}

	cmdw_hop->ses = type_query->ses;
	cmdw_hop->download_uri = get_uri_reference(type_query->uri);
	cmdw_hop->real_file = codw_hop->real_file;
	codw_hop->real_file = NULL;

	common_download_do(term, fd, cmdw_hop, flags);
}

/*! continue_download() passes this function as a ::cdf_callback_T to
 * create_download_file().
 *
 * @relates codw_hop */
static void
continue_download_do(struct terminal *term, int fd, void *data,
		     download_flags_T flags)
{
	struct codw_hop *codw_hop = (struct codw_hop *)data;
	struct file_download *file_download = NULL;
	struct type_query *type_query;

	assert(codw_hop);
	assert(codw_hop->type_query);
	assert(codw_hop->type_query->uri);
	assert(codw_hop->type_query->ses);

	type_query = codw_hop->type_query;
	if (!codw_hop->real_file) goto cancel;

	if (flags & DOWNLOAD_RESUME_SELECTED) {
		transform_codw_to_cmdw(term, fd, codw_hop, flags);
		fd = -1; /* ownership transfer */
		goto cancel;
	}

	file_download = init_file_download(type_query->uri, type_query->ses,
					   codw_hop->real_file, fd);
	if (!file_download) goto cancel;
	/* If init_file_download succeeds, it takes ownership of
	 * codw_hop->real_file and fd.  */
	codw_hop->real_file = NULL;
	fd = -1;

	if (type_query->dgi && type_query->external_handler) {
		file_download->external_handler = type_query->external_handler;
		file_download->file = codw_hop->file;
		file_download->inpext = null_or_stracpy(type_query->inpext);
		file_download->outext = null_or_stracpy(type_query->outext);
		file_download->dgi = type_query->dgi;
		file_download->delete_ = 1;
		/* change owners a few lines above */
		codw_hop->file = NULL;
		type_query->external_handler = NULL;
	} else if (type_query->external_handler) {
		file_download->external_handler = subst_file(type_query->external_handler,
							     codw_hop->file,
							     type_query->uri->string);
		file_download->delete_ = 1;
		file_download->copiousoutput = type_query->copiousoutput;
		mem_free(codw_hop->file);
		mem_free_set(&type_query->external_handler, NULL);
	}

	file_download->block = !!type_query->block;

	/* Done here and not in init_file_download() so that the external
	 * handler can become initialized. */
	display_download(term, file_download, type_query->ses);

	move_download(&type_query->download, &file_download->download, PRI_DOWNLOAD);
	done_type_query(type_query);

	mem_free(codw_hop);
	return;

cancel:
	mem_free_if(codw_hop->real_file);
	if (fd != -1) close(fd);
	if (type_query->external_handler) mem_free_if(codw_hop->file);
	tp_cancel(type_query);
	mem_free(codw_hop);
}

/** When asked what to do with a file, the user chose to download it
 * to a local file named @a file.
 * Or an external handler was selected, in which case
 * type_query.external_handler is non-NULL and @a file does not
 * matter because this function will generate a name.
 *
 * tp_save() passes this function as the @c std callback to query_file().
 *
 * @relates codw_hop */
static void
continue_download(void *data, char *file)
{
	struct type_query *type_query = (struct type_query *)data;
	struct codw_hop *codw_hop = (struct codw_hop *)mem_calloc(1, sizeof(*codw_hop));

	if (!codw_hop) {
		tp_cancel(type_query);
		return;
	}

	if (type_query->external_handler) {
		file = get_temp_name(type_query->uri);
		if (!file) {
			mem_free(codw_hop);
			tp_cancel(type_query);
			return;
		}
	}

	codw_hop->type_query = type_query;
	codw_hop->file = file;

	kill_downloads_to_file(file);

	create_download_file(type_query->ses->tab->term, file,
			     &codw_hop->real_file,
			     type_query->external_handler
			     ? DOWNLOAD_RESUME_ALLOWED |
			       DOWNLOAD_EXTERNAL |
			       DOWNLOAD_OVERWRITE
			     : DOWNLOAD_RESUME_ALLOWED,
			     continue_download_do, codw_hop);
}


/*! @relates type_query */
static struct type_query *
find_type_query(struct session *ses)
{
	struct type_query *type_query;

	foreach (type_query, ses->type_queries)
		if (compare_uri(type_query->uri, ses->loading_uri, 0))
			return type_query;

	return NULL;
}

/** Prepare to ask the user what to do with a file, but don't display
 * the window yet.  To display it, do_type_query() must be called
 * separately.  setup_download_handler() takes care of that.
 *
 * @relates type_query */
static struct type_query *
init_type_query(struct session *ses, struct download *download,
	struct cache_entry *cached)
{
	struct type_query *type_query;

	type_query = (struct type_query *)mem_calloc(1, sizeof(*type_query));
	if (!type_query) return NULL;

	type_query->uri = get_uri_reference(ses->loading_uri);
	type_query->ses = ses;
	type_query->target_frame = null_or_stracpy(ses->task.target.frame);

	type_query->cached = cached;
	type_query->cgi = cached->cgi;
	object_lock(type_query->cached);

	move_download(download, &type_query->download, PRI_MAIN);
	download->state = connection_state(S_OK);

	add_to_list(ses->type_queries, type_query);

	return type_query;
}

/** Cancel any download started for @a type_query, remove the structure
 * from the session.type_queries list, and free it.
 *
 * @relates type_query */
void
done_type_query(struct type_query *type_query)
{
	/* Unregister any active download */
	cancel_download(&type_query->download, 0);

	object_unlock(type_query->cached);
	done_uri(type_query->uri);
	mem_free_if(type_query->inpext);
	mem_free_if(type_query->outext);
	mem_free_if(type_query->external_handler);
	mem_free_if(type_query->target_frame);
	del_from_list(type_query);
	mem_free(type_query);
}


/** The user chose "Cancel" when asked what to do with a file,
 * or the type query was cancelled for some other reason.
 *
 * do_type_query() and bittorrent_query_callback() pass this function
 * as a ::done_handler_T to add_dlg_ok_button(), and tp_save() passes
 * this function as a @c cancel callback to query_file().
 *
 * @relates type_query */
void
tp_cancel(void *data)
{
	struct type_query *type_query = (struct type_query *)data;

	/* XXX: Should we really abort? (1 vs 0 as the last param) --pasky */
	cancel_download(&type_query->download, 1);
	done_type_query(type_query);
}


/** The user chose "Save" when asked what to do with a file.
 * Now ask her where to save the file.
 *
 * do_type_query() and bittorrent_query_callback() pass this function
 * as a ::done_handler_T to add_dlg_ok_button().
 *
 * @relates type_query */
void
tp_save(struct type_query *type_query)
{
	mem_free_set(&type_query->external_handler, NULL);
	query_file(type_query->ses, type_query->uri, type_query, continue_download, tp_cancel, 1);
}

/** The user chose "Show header" when asked what to do with a file.
 *
 * do_type_query() passes this function as a ::widget_handler_T to
 * add_dlg_button().  Unlike with add_dlg_ok_button(), pressing this
 * button does not close the dialog box.  This way, the user can
 * first examine the header and then choose what to do.
 *
 * @relates type_query */
static widget_handler_status_T
tp_show_header(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	struct type_query *type_query = (struct type_query *)widget_data->widget->data;

	cached_header_dialog(type_query->ses, type_query->cached);

	return EVENT_PROCESSED;
}


/** The user chose "Display" when asked what to do with a file,
 * or she chose "Open" and there is no external handler.
 *
 * do_type_query() and bittorrent_query_callback() pass this function
 * as a ::done_handler_T to add_dlg_ok_button().
 *
 * @bug FIXME: We need to modify this function to take frame data
 * instead, as we want to use this function for frames as well (now,
 * when frame has content type text/plain, it is ignored and displayed
 * as HTML).
 *
 * @relates type_query */
void
tp_display(struct type_query *type_query)
{
	struct view_state *vs;
	struct session *ses = type_query->ses;
	struct uri *loading_uri = ses->loading_uri;
	char *target_frame = null_or_stracpy(ses->task.target.frame);

	ses->loading_uri = type_query->uri;
	mem_free_set(&ses->task.target.frame, null_or_stracpy(type_query->target_frame));
	vs = ses_forward(ses, /* type_query->frame */ 0);
	if (vs) vs->plain = 1;
	ses->loading_uri = loading_uri;
	mem_free_set(&ses->task.target.frame, target_frame);

	if (/* !type_query->frame */ 1) {
		struct download *old = &type_query->download;
		struct download *new_ = &cur_loc(ses)->download;

		new_->callback = (download_callback_T *) doc_loading_callback;
		new_->data = ses;

		move_download(old, new_, PRI_MAIN);
	}

	display_timer(ses);
	done_type_query(type_query);
}

/** The user chose "Open" when asked what to do with a file.
 * Or an external handler was found and it has been configured
 * to run without asking.
 *
 * do_type_query() passes this function as a ::done_handler_T to
 * add_dlg_ok_button().
 *
 * @relates type_query */
static void
tp_open(struct type_query *type_query)
{
	if (!type_query->external_handler || !*type_query->external_handler) {
		tp_display(type_query);
		return;
	}

	if (type_query->uri->protocol == PROTOCOL_FILE && !type_query->cgi) {
		char *file = get_uri_string(type_query->uri, URI_PATH);
		char *handler = NULL;

		if (type_query->dgi) {
			if (file) {
				decode_uri(file);
			}
			exec_later_dgi(type_query->ses, type_query->external_handler, file,
				type_query->inpext, type_query->outext, 0);
			mem_free_if(file);
			done_type_query(type_query);
			return;
		}

		if (file) {
			decode_uri(file);
			handler = subst_file(type_query->external_handler,
				file, file);
			mem_free(file);
		}

		if (handler) {
			if (type_query->copiousoutput) {
				exec_later(type_query->ses, handler, NULL);
			} else {
				exec_on_terminal(type_query->ses->tab->term,
						 handler, "", type_query->block ?
						 TERM_EXEC_FG : TERM_EXEC_BG);
			}
			mem_free(handler);
		}

		done_type_query(type_query);
		return;
	} else { // Check in cache
		char *url = get_uri_string(type_query->uri, URI_PUBLIC);

		if (url) {
			char *filename = check_url_tempfiles(url);

			if (filename) {
				if (file_can_read(filename)) {
					char *handler = subst_file(type_query->external_handler, filename, filename);

					if (handler) {
						if (type_query->copiousoutput) {
							exec_later(type_query->ses, handler, NULL);
						} else {
							exec_on_terminal(type_query->ses->tab->term,
							handler, "", type_query->block ?
							TERM_EXEC_FG : TERM_EXEC_BG);
						}
						mem_free(handler);
					}
					mem_free(filename);
					done_type_query(type_query);
					mem_free(url);
					return;
				}
				mem_free(filename);
			}
			mem_free(url);
		}
	}
	continue_download(type_query, (char *)(""));
}


/*! Ask the user what to do with a file.
 *
 * This function does not support BitTorrent downloads.
 * For those, query_bittorrent_dialog() must be called instead.
 * setup_download_handler() takes care of this.
 *
 * @relates type_query */
static void
do_type_query(struct type_query *type_query, char *ct, struct mime_handler *handler)
{
	/* [gettext_accelerator_context(.do_type_query)] */
	struct string filename;
	const char *description;
	const char *desc_sep;
	char *format, *text, *title;
	struct dialog *dlg;
#define TYPE_QUERY_WIDGETS_COUNT 8
	int widgets = TYPE_QUERY_WIDGETS_COUNT;
	struct terminal *term = type_query->ses->tab->term;
	struct memory_list *ml;
	struct dialog_data *dlg_data;
	int selected_widget;

	mem_free_set(&type_query->external_handler, NULL);

	if (handler) {
		type_query->block = handler->block;
		type_query->copiousoutput = handler->copiousoutput;
		type_query->dgi = handler->dgi;
		type_query->inpext = null_or_stracpy(handler->inpext);
		type_query->outext = null_or_stracpy(handler->outext);
		if (!handler->ask) {
			type_query->external_handler = stracpy(handler->program);
			tp_open(type_query);
			return;
		}

		/* Start preparing for the type query dialog. */
		description = handler->description;
		desc_sep = *description ? "; " : "";
		title = N_("What to do?");

	} else {
		title = N_("Unknown type");
		description = "";
		desc_sep = "";
	}

	dlg = calloc_dialog(TYPE_QUERY_WIDGETS_COUNT, MAX_STR_LEN * 2);
	if (!dlg) return;

	if (init_string(&filename)) {
		add_mime_filename_to_string(&filename, type_query->uri);

		/* Let's make the filename pretty for display & save */
		/* TODO: The filename can be the empty string here. See bug 396. */
#ifdef CONFIG_UTF8
		if (term->utf8_cp)
			decode_uri_string(&filename);
		else
#endif /* CONFIG_UTF8 */
			decode_uri_string_for_display(&filename);
	}

	text = get_dialog_offset(dlg, TYPE_QUERY_WIDGETS_COUNT);
	/* For "default directory index pages" with wrong content-type
	 * the filename can be NULL, e.g. http://www.spamhaus.org in bug 396. */
	if (filename.length) {
		format = _("What would you like to do with the file '%s' (type: %s%s%s)?", term);
		snprintf(text, MAX_STR_LEN, format, filename.source, ct, desc_sep, description);
	} else {
		format = _("What would you like to do with the file (type: %s%s%s)?", term);
		snprintf(text, MAX_STR_LEN, format, ct, desc_sep, description);
	}

	done_string(&filename);

	dlg->title = _(title, term);
	dlg->layouter = generic_dialog_layouter;
	dlg->layout.padding_top = 1;
	dlg->layout.fit_datalen = 1;
	dlg->udata2 = type_query;

	add_dlg_text(dlg, text, ALIGN_LEFT, 0);

	/* Add input field or text widget with info about the program handler. */
	if (!get_cmd_opt_bool("anonymous")) {
		char *field = (char *)mem_calloc(1, MAX_STR_LEN);

		if (!field) {
			mem_free(dlg);
			return;
		}

		if (handler && handler->program) {
			safe_strncpy(field, handler->program, MAX_STR_LEN);
		}

		/* xgettext:no-c-format */
		add_dlg_field(dlg,
			_("Program ('%f' will be replaced by the filename, "
			  "'%u' by the uri)", term),
			0, 0, NULL, MAX_STR_LEN, field, NULL);
		type_query->external_handler = field;

		add_dlg_checkbox(dlg, _("Block the terminal", term), &type_query->block);
		selected_widget = 3;

	} else if (handler) {
		char *field = text + MAX_STR_LEN;

		format = _("The file will be opened with the program '%s'.", term);
		snprintf(field, MAX_STR_LEN, format, handler->program);
		add_dlg_text(dlg, field, ALIGN_LEFT, 0);

		type_query->external_handler = stracpy(handler->program);
		if (!type_query->external_handler) {
			mem_free(dlg);
			return;
		}

		widgets--;
		selected_widget = 2;

	} else {
		widgets -= 2;
		selected_widget = 1;
	}

	/* Add buttons if they are both usable and allowed. */

	if (!get_cmd_opt_bool("anonymous") || handler) {
		add_dlg_ok_button(dlg, _("~Open", term), B_ENTER,
				  (done_handler_T *) tp_open, type_query);
	} else {
		widgets--;
	}

	if (!get_cmd_opt_bool("anonymous")) {
		add_dlg_ok_button(dlg, _("Sa~ve", term), B_ENTER,
				  (done_handler_T *) tp_save, type_query);
	} else {
		widgets--;
	}

	add_dlg_ok_button(dlg, _("~Display", term), B_ENTER,
			  (done_handler_T *) tp_display, type_query);

	if (type_query->cached && type_query->cached->head) {
		add_dlg_button(dlg, _("Show ~header", term), B_ENTER,
			       tp_show_header, type_query);
	} else {
		widgets--;
	}

	add_dlg_ok_button(dlg, _("~Cancel", term), B_ESC,
			  (done_handler_T *) tp_cancel, type_query);

	add_dlg_end(dlg, widgets);

	ml = getml(dlg, (void *) NULL);
	if (!ml) {
		/* XXX: Assume that the allocated @external_handler will be
		 * freed when releasing the @type_query. */
		mem_free(dlg);
		return;
	}

	dlg_data = do_dialog(term, dlg, ml);
	/* Don't focus the text field; we want the user to be able
	 * to select a button by typing the first letter of its label
	 * without having to first leave the text field. */
	if (dlg_data) {
		select_widget_by_id(dlg_data, selected_widget);
	}
}

struct {
	const char *type;
	unsigned int plain:1;
} static const known_types[] = {
	{ "text/html",			0 },
	{ "text/plain",			1 },
	{ "text/gemini",		0 },
	{ "application/xhtml+xml",	0 }, /* RFC 3236 */
#ifdef CONFIG_DOM
	{ "application/docbook+xml",	1 },
	{ "application/rss+xml",	0 },
	{ "application/xbel+xml",	1 },
	{ "application/xbel",		1 },
	{ "application/x-xbel",		1 },
#endif
	{ NULL,				1 },
};

static const char *compressed_types[] = {
#ifdef CONFIG_GZIP
	"application/gzip",
#endif
#ifdef CONFIG_ZSTD
	"application/zstd",
#endif
#ifdef CONFIG_LZMA
	"application/x-xz",
#endif
#ifdef CONFIG_BZIP2
	"application/x-bzip2",
#endif
	NULL
};

/*! @relates type_query */
int
setup_download_handler(struct session *ses, struct download *loading,
		       struct cache_entry *cached, int frame)
{
	struct mime_handler *handler;
	struct view_state *vs;
	struct type_query *type_query;
	char *ctype = get_content_type(cached);
	int plaintext = 1;
	int ret = 0;
	int xwin, i;

	if (!ctype || !*ctype)
		goto plaintext_follow;

	for (i = 0; known_types[i].type; i++) {
		if (c_strcasecmp(ctype, known_types[i].type))
			continue;

		plaintext = known_types[i].plain;
		goto plaintext_follow;
	}
	xwin = ses->tab->term->environment & ENV_XWIN;
	handler = get_mime_type_handler(ctype, xwin);

	if (!handler) {
		if (strlen(ctype) >= 4 && !c_strncasecmp(ctype, "text", 4)) {
			goto plaintext_follow;
		}
		for (i = 0; compressed_types[i]; i++) {
			if (c_strcasecmp(ctype, compressed_types[i])) {
				continue;
			}
			if (get_opt_bool("document.download.compressed_as_plain", ses)) {
				goto plaintext_follow;
			}
		}
	}

	type_query = find_type_query(ses);
	if (type_query) {
		ret = 1;
	} else {
		type_query = init_type_query(ses, loading, cached);
		if (type_query) {
			ret = 1;
#ifdef CONFIG_BITTORRENT
			/* A terrible waste of a good MIME handler here, but we want
			 * to use the type_query this is easier. */
			if ((!c_strcasecmp(ctype, "application/x-bittorrent")
				|| !c_strcasecmp(ctype, "application/x-torrent"))
			    && !get_cmd_opt_bool("anonymous"))
				query_bittorrent_dialog(type_query);
			else
#endif
				do_type_query(type_query, ctype, handler);
		}
	}

	mem_free_if(handler);

	return ret;

plaintext_follow:
	vs = ses_forward(ses, frame);
	if (vs) vs->plain = plaintext;
	return 0;
}
