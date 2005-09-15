/* Downloads managment */
/* $Id: download.c,v 1.338.2.9 2005/05/13 20:06:01 jonas Exp $ */

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
#include "intl/gettext/libintl.h"
#include "mime/mime.h"
#include "osdep/osdep.h"
#include "protocol/date.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/download.h"
#include "sched/error.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/session.h"
#include "sched/task.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/file.h"
#include "util/lists.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"
#include "util/ttime.h"


/* TODO: tp_*() should be in separate file, I guess? --pasky */


INIT_LIST_HEAD(downloads);


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

static struct file_download *
init_file_download(struct uri *uri, struct session *ses, unsigned char *file, int fd)
{
	struct file_download *file_download;

	file_download = mem_calloc(1, sizeof(*file_download));
	if (!file_download) return NULL;

	/* Actually we could allow fragments in the URI and just change all the
	 * places that compares and shows the URI, but for now it is much
	 * easier this way. */
	file_download->uri = get_composed_uri(uri, URI_BASE);
	if (!file_download->uri) {
		mem_free(file_download);
		return NULL;
	}

	file_download->box_item = add_listbox_leaf(&download_browser, NULL,
						   file_download);

	file_download->file = file;
	file_download->handle = fd;

	file_download->download.callback = (void (*)(struct download *, void *)) download_data;
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

	if (file_download->box_item)
		done_listbox_item(&download_browser, file_download->box_item);
	if (file_download->dlg_data)
		cancel_dialog(file_download->dlg_data, NULL);
	if (is_in_progress_state(file_download->download.state))
		change_connection(&file_download->download, NULL, PRI_CANCEL,
				  file_download->stop);
	if (file_download->uri) done_uri(file_download->uri);

	if (file_download->handle != -1) {
		prealloc_truncate(file_download->handle,
				  file_download->last_pos);
		close(file_download->handle);
	}

	mem_free_if(file_download->external_handler);
	if (file_download->file) {
		if (file_download->delete) unlink(file_download->file);
		mem_free(file_download->file);
	}
	del_from_list(file_download);
	mem_free(file_download);
}


static void
kill_downloads_to_file(unsigned char *file)
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
		abort_download(downloads.next);
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


static void
download_error_dialog(struct file_download *file_download, int saved_errno)
{
	unsigned char *emsg = (unsigned char *) strerror(saved_errno);
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
		file_download->last_pos = file_download->download.progress->seek;
		file_download->download.progress->seek = 0;
		/* This is exclusive with the prealloc, thus we can perform
		 * this in front of that thing safely. */
		if (lseek(file_download->handle, file_download->last_pos, SEEK_SET) < 0) {
			download_error_dialog(file_download, errno);
			return 0;
		}
	}

	foreach (frag, cached->frag) {
		int remain = file_download->last_pos - frag->offset;
		int *h = &file_download->handle;
		int w;

		if (remain < 0 || frag->length <= remain)
			continue;

#ifdef USE_OPEN_PREALLOC
		if (!file_download->last_pos
		    && (!file_download->download.prg
			|| file_download->download.progress->size > 0)) {
			close(*h);
			*h = open_prealloc(file_download->file,
					   O_CREAT|O_WRONLY|O_TRUNC,
					   0666,
					   file_download->download.prg
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

		file_download->last_pos += w;
	}

	return 1;
}

static void
abort_download_and_beep(struct file_download *file_download, struct terminal *term)
{
	if (term && get_opt_int("document.download.notify_bell")
		    + file_download->notify >= 2) {
		beep_terminal(term);
	}

	abort_download(file_download);
}

static void
download_data_store(struct download *download, struct file_download *file_download)
{
	struct terminal *term = file_download->term;

	if (!term) {
		/* No term here, so no beep. --Zas */
		abort_download(file_download);
		return;
	}

	if (is_in_progress_state(download->state)) {
		if (file_download->dlg_data)
			redraw_dialog(file_download->dlg_data, 1);
		return;
	}

	if (download->state != S_OK) {
		unsigned char *errmsg = get_err_msg(download->state, term);
		unsigned char *url = get_uri_string(file_download->uri, URI_PUBLIC);

		if (url) {
			info_box(term, MSGBOX_FREE_TEXT,
				 N_("Download error"), ALIGN_CENTER,
				 msg_text(term, N_("Error downloading %s:\n\n%s"),
					  url, errmsg));
			mem_free(url);
		}

		abort_download_and_beep(file_download, term);
		return;
	}

	if (file_download->external_handler) {
		prealloc_truncate(file_download->handle,
				  file_download->last_pos);
		close(file_download->handle);
		file_download->handle = -1;
		exec_on_terminal(term, file_download->external_handler,
				 file_download->file,
				 !!file_download->external_handler_flags);
		file_download->delete = 0;
		abort_download_and_beep(file_download, term);
		return;
	}

	if (file_download->notify) {
		unsigned char *url = get_uri_string(file_download->uri, URI_PUBLIC);

		/* This is apparently a little racy. Deleting the box item will
		 * update the download browser _after_ the notification dialog
		 * has been drawn whereby it will be hidden. This should make
		 * the download browser update before launcing any
		 * notification. */
		if (file_download->box_item) {
			done_listbox_item(&download_browser, file_download->box_item);
			file_download->box_item = NULL;
		}

		if (url) {
			info_box(term, MSGBOX_FREE_TEXT,
				 N_("Download"), ALIGN_CENTER,
				 msg_text(term, N_("Download complete:\n%s"), url));
			mem_free(url);
		}
	}

	if (file_download->remotetime
	    && get_opt_bool("document.download.set_original_time")) {
		struct utimbuf foo;

		foo.actime = foo.modtime = file_download->remotetime;
		utime(file_download->file, &foo);
	}

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
		if (is_in_progress_state(download->state))
			change_connection(&file_download->download, NULL, PRI_CANCEL, 0);

		assertm(compare_uri(cached->uri, file_download->uri, 0),
			"Redirecting using bad base URI");

		done_uri(file_download->uri);

		file_download->uri = get_uri_reference(cached->redirect);
		file_download->download.state = S_WAIT_REDIR;

		if (file_download->dlg_data)
			redraw_dialog(file_download->dlg_data, 1);

		load_uri(file_download->uri, cached->uri, &file_download->download,
			 PRI_DOWNLOAD, CACHE_MODE_NORMAL,
			 download->progress ? download->progress->start : 0);

		return;
	}

	if (!write_cache_entry_to_file(cached, file_download)) {
		detach_connection(download, file_download->last_pos);
		abort_download(file_download);
		return;
	}

	detach_connection(download, file_download->last_pos);
	download_data_store(download, file_download);
}


/* XXX: We assume that resume is everytime zero in lun's callbacks. */
struct lun_hop {
	struct terminal *term;
	unsigned char *ofile, *file;

	void (*callback)(struct terminal *, unsigned char *, void *, int);
	void *data;
};

static void
lun_alternate(struct lun_hop *lun_hop)
{
	lun_hop->callback(lun_hop->term, lun_hop->file, lun_hop->data, 0);
	mem_free_if(lun_hop->ofile);
	mem_free(lun_hop);
}

static void
lun_overwrite(struct lun_hop *lun_hop)
{
	lun_hop->callback(lun_hop->term, lun_hop->ofile, lun_hop->data, 0);
	mem_free_if(lun_hop->file);
	mem_free(lun_hop);
}

static void
lun_resume(struct lun_hop *lun_hop)
{
	lun_hop->callback(lun_hop->term, lun_hop->ofile, lun_hop->data, 1);
	mem_free_if(lun_hop->file);
	mem_free(lun_hop);
}

static void
lun_cancel(struct lun_hop *lun_hop)
{
	lun_hop->callback(lun_hop->term, NULL, lun_hop->data, 0);
	mem_free_if(lun_hop->ofile);
	mem_free_if(lun_hop->file);
	mem_free(lun_hop);
}

static void
lookup_unique_name(struct terminal *term, unsigned char *ofile, int resume,
		   void (*callback)(struct terminal *, unsigned char *, void *, int),
		   void *data)
{
	struct lun_hop *lun_hop;
	unsigned char *file;
	int overwrite;

	ofile = expand_tilde(ofile);

	/* Minor code duplication to prevent useless call to get_opt_int()
	 * if possible. --Zas */
	if (resume) {
		callback(term, ofile, data, resume);
		return;
	}

	/* !overwrite means always silently overwrite, which may be admitelly
	 * indeed a little confusing ;-) */
	overwrite = get_opt_int("document.download.overwrite");
	if (!overwrite) {
		/* Nothing special to do... */
		callback(term, ofile, data, resume);
		return;
	}

	/* Check if file is a directory, and use a default name if it's the
	 * case. */
	if (file_is_dir(ofile)) {
		info_box(term, MSGBOX_FREE_TEXT,
			 N_("Download error"), ALIGN_CENTER,
			 msg_text(term, N_("'%s' is a directory."),
				  ofile));
		mem_free(ofile);
		callback(term, NULL, data, 0);
		return;
	}

	/* Check if the file already exists (file != ofile). */
	file = get_unique_name(ofile);

	if (!file || overwrite == 1 || file == ofile) {
		/* Still nothing special to do... */
		if (file != ofile) mem_free(ofile);
		callback(term, file, data, 0);
		return;
	}

	/* overwrite == 2 (ask) and file != ofile (=> original file already
	 * exists) */

	lun_hop = mem_calloc(1, sizeof(*lun_hop));
	if (!lun_hop) {
		if (file != ofile) mem_free(file);
		mem_free(ofile);
		callback(term, NULL, data, 0);
		return;
	}
	lun_hop->term = term;
	lun_hop->ofile = ofile;
	lun_hop->file = (file != ofile) ? file : stracpy(ofile);
	lun_hop->callback = callback;
	lun_hop->data = data;

	msg_box(term, NULL, MSGBOX_FREE_TEXT,
		N_("File exists"), ALIGN_CENTER,
		msg_text(term, N_("This file already exists:\n"
			"%s\n\n"
			"The alternative filename is:\n"
			"%s"),
			empty_string_or_(lun_hop->ofile),
			empty_string_or_(file)),
		lun_hop, 4,
		N_("Sa~ve under the alternative name"), lun_alternate, B_ENTER,
		N_("~Overwrite the original file"), lun_overwrite, 0,
		N_("~Resume download of the original file"), lun_resume, 0,
		N_("~Cancel"), lun_cancel, B_ESC);
}


struct cdf_hop {
	unsigned char **real_file;
	int safe;

	void (*callback)(struct terminal *, int, void *, int);
	void *data;
};

static void
create_download_file_do(struct terminal *term, unsigned char *file, void *data,
			int resume)
{
	struct cdf_hop *cdf_hop = data;
	unsigned char *wd;
	int h = -1;
	int saved_errno;
#ifdef NO_FILE_SECURITY
	int sf = 0;
#else
	int sf = cdf_hop->safe;
#endif

	if (!file) goto finish;

	wd = get_cwd();
	set_cwd(term->cwd);

	/* O_APPEND means repositioning at the end of file before each write(),
	 * thus ignoring seek()s and that can hide mysterious bugs. IMHO.
	 * --pasky */
	h = open(file, O_CREAT | O_WRONLY | (resume ? 0 : O_TRUNC)
			| (sf && !resume ? O_EXCL : 0),
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

		if (!cdf_hop->safe) {
			unsigned char *download_dir = get_opt_str("document.download.directory");
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
	cdf_hop->callback(term, h, cdf_hop->data, resume);
	mem_free(cdf_hop);
}

void
create_download_file(struct terminal *term, unsigned char *fi,
		     unsigned char **real_file, int safe, int resume,
		     void (*callback)(struct terminal *, int, void *, int),
		     void *data)
{
	struct cdf_hop *cdf_hop = mem_calloc(1, sizeof(*cdf_hop));
	unsigned char *wd;

	if (!cdf_hop) {
		callback(term, -1, data, 0);
		return;
	}

	cdf_hop->real_file = real_file;
	cdf_hop->safe = safe;
	cdf_hop->callback = callback;
	cdf_hop->data = data;

	/* FIXME: The wd bussiness is probably useless here? --pasky */
	wd = get_cwd();
	set_cwd(term->cwd);

	/* Also the tilde will be expanded here. */
	lookup_unique_name(term, fi, resume, create_download_file_do, cdf_hop);

	if (wd) {
		set_cwd(wd);
		mem_free(wd);
	}
}


static unsigned char *
get_temp_name(struct uri *uri)
{
	struct string name;
	unsigned char *extension;
	/* FIXME
	 * We use tempnam() here, which is unsafe (race condition), for now.
	 * This should be changed at some time, but it needs an in-depth work
	 * of whole download code. --Zas */
	unsigned char *nm = tempnam(NULL, ELINKS_TEMPNAME_PREFIX);

	if (!nm) return NULL;

	if (!init_string(&name)) {
		free(nm);
		return NULL;
	}

	add_to_string(&name, nm);
	free(nm);

	extension = get_extension_from_uri(uri);
	if (extension) {
		add_shell_safe_to_string(&name, extension, strlen(extension));
		mem_free(extension);
	}

	return name.source;
}


static unsigned char *
subst_file(unsigned char *prog, unsigned char *file)
{
	struct string name;

	if (!init_string(&name)) return NULL;

	while (*prog) {
		int p;

		for (p = 0; prog[p] && prog[p] != '%'; p++);

		add_bytes_to_string(&name, prog, p);
		prog += p;

		if (*prog == '%') {
#if defined(HAVE_CYGWIN_CONV_TO_FULL_WIN32_PATH)
#ifdef MAX_PATH
			unsigned char new_path[MAX_PATH];
#else
			unsigned char new_path[1024];
#endif

			cygwin_conv_to_full_win32_path(file, new_path);
			add_to_string(&name, new_path);
#else
			add_to_string(&name, file);
#endif
			prog++;
		}
	}

	return name.source;
}


struct cmdw_hop {
	struct session *ses;
	unsigned char *real_file;
};

static void
common_download_do(struct terminal *term, int fd, void *data, int resume)
{
	struct cmdw_hop *cmdw_hop = data;
	unsigned char *file = cmdw_hop->real_file;
	struct file_download *file_download;
	struct session *ses = cmdw_hop->ses;
	struct stat buf;

	mem_free(cmdw_hop);

	if (!file || fstat(fd, &buf)) return;

	file_download = init_file_download(ses->download_uri, ses, file, fd);
	if (!file_download) return;

	file_download->last_pos = resume ? (int) buf.st_size : 0;

	display_download(ses->tab->term, file_download, ses);

	load_uri(file_download->uri, ses->referrer, &file_download->download,
		 PRI_DOWNLOAD, CACHE_MODE_NORMAL, file_download->last_pos);
}

static void
common_download(struct session *ses, unsigned char *file, int resume)
{
	struct cmdw_hop *cmdw_hop;

	if (!ses->download_uri) return;

	cmdw_hop = mem_calloc(1, sizeof(*cmdw_hop));
	if (!cmdw_hop) return;
	cmdw_hop->ses = ses;

	kill_downloads_to_file(file);

	create_download_file(ses->tab->term, file, &cmdw_hop->real_file, 0,
			     resume, common_download_do, cmdw_hop);
}

void
start_download(void *ses, unsigned char *file)
{
	common_download(ses, file, 0);
}

void
resume_download(void *ses, unsigned char *file)
{
	common_download(ses, file, 1);
}


struct codw_hop {
	struct type_query *type_query;
	unsigned char *real_file;
	unsigned char *file;
};

static void tp_cancel(void *);

static void
continue_download_do(struct terminal *term, int fd, void *data, int resume)
{
	struct codw_hop *codw_hop = data;
	struct file_download *file_download = NULL;
	struct type_query *type_query;

	assert(codw_hop);
	assert(codw_hop->type_query);
	assert(codw_hop->type_query->uri);
	assert(codw_hop->type_query->ses);

	type_query = codw_hop->type_query;
	if (!codw_hop->real_file) goto cancel;

	file_download = init_file_download(type_query->uri, type_query->ses,
					   codw_hop->real_file, fd);
	if (!file_download) goto cancel;

	if (type_query->external_handler) {
		file_download->external_handler = subst_file(type_query->external_handler,
							     codw_hop->file);
		file_download->delete = 1;
		mem_free(codw_hop->file);
		mem_free_set(&type_query->external_handler, NULL);
	}

	file_download->external_handler_flags = type_query->external_handler_flags;

	/* Done here and not in init_file_download() so that the external
	 * handler can become initialized. */
	display_download(term, file_download, type_query->ses);

	change_connection(&type_query->download, &file_download->download, PRI_DOWNLOAD, 0);
	done_type_query(type_query);

	mem_free(codw_hop);
	return;

cancel:
	if (type_query->external_handler) mem_free_if(codw_hop->file);
	tp_cancel(type_query);
	mem_free(codw_hop);
}

static void
continue_download(void *data, unsigned char *file)
{
	struct type_query *type_query = data;
	struct codw_hop *codw_hop = mem_calloc(1, sizeof(*codw_hop));

	if (!codw_hop) {
		tp_cancel(type_query);
		return;
	}

	if (type_query->external_handler) {
		/* FIXME: get_temp_name() calls tempnam(). --Zas */
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
			     !!type_query->external_handler, 0,
			     continue_download_do, codw_hop);
}


static struct type_query *
init_type_query(struct session *ses, struct download *download,
	struct cache_entry *cached)
{
	struct type_query *type_query;

	/* There can be only one ... */
	foreach (type_query, ses->type_queries)
		if (compare_uri(type_query->uri, ses->loading_uri, 0))
			return NULL;

	type_query = mem_calloc(1, sizeof(*type_query));
	if (!type_query) return NULL;

	type_query->uri = get_uri_reference(ses->loading_uri);
	type_query->ses = ses;
	type_query->target_frame = null_or_stracpy(ses->task.target_frame);

	type_query->cached = cached;
	object_lock(type_query->cached);

	change_connection(download, &type_query->download, PRI_MAIN, 0);
	download->state = S_OK;

	add_to_list(ses->type_queries, type_query);

	return type_query;
}

void
done_type_query(struct type_query *type_query)
{
	/* Unregister any active download */
	if (is_in_progress_state(type_query->download.state))
		change_connection(&type_query->download, NULL, PRI_CANCEL, 0);

	object_unlock(type_query->cached);
	done_uri(type_query->uri);
	mem_free_if(type_query->external_handler);
	mem_free_if(type_query->target_frame);
	del_from_list(type_query);
	mem_free(type_query);
}


static void
tp_cancel(void *data)
{
	struct type_query *type_query = data;
	/* XXX: Should we really abort? (1 vs 0 as the last param) --pasky */
	change_connection(&type_query->download, NULL, PRI_CANCEL, 1);
	done_type_query(type_query);
}


static void
tp_save(struct type_query *type_query)
{
	mem_free_set(&type_query->external_handler, NULL);
	query_file(type_query->ses, type_query->uri, type_query, continue_download, tp_cancel, 1);
}


static void
tp_open(struct type_query *type_query)
{
	continue_download(type_query, "");
}


static void
tp_show_header(struct type_query *type_query)
{
	cached_header_dialog(type_query->ses, type_query->cached);

	done_type_query(type_query);
}


/* FIXME: We need to modify this function to take frame data instead, as we
 * want to use this function for frames as well (now, when frame has content
 * type text/plain, it is ignored and displayed as HTML). */
static void
tp_display(struct type_query *type_query)
{
	struct view_state *vs;
	struct session *ses = type_query->ses;
	struct uri *loading_uri = ses->loading_uri;
	unsigned char *target_frame = ses->task.target_frame;

	ses->loading_uri = type_query->uri;
	ses->task.target_frame = type_query->target_frame;
	vs = ses_forward(ses, type_query->frame);
	if (vs) vs->plain = 1;
	ses->loading_uri = loading_uri;
	ses->task.target_frame = target_frame;

	if (!type_query->frame) {
		struct download *old = &type_query->download;
		struct download *new = &cur_loc(ses)->download;

		new->callback = (void (*)(struct download *, void *))
				doc_loading_callback;
		new->data = ses;

		if (is_in_progress_state(old->state))
			change_connection(old, new, PRI_MAIN, 0);
		else
			new->state = old->state;
	}

	display_timer(ses);
	done_type_query(type_query);
}


static void
do_type_query(struct type_query *type_query, unsigned char *ct, struct mime_handler *handler)
{
	struct string filename;

	mem_free_set(&type_query->external_handler, NULL);

	if (handler) {
		type_query->external_handler = stracpy(handler->program);
		type_query->external_handler_flags = handler->block;
		if (!handler->ask) {
			tp_open(type_query);
			return;
		}
	}

	if (init_string(&filename))
		add_mime_filename_to_string(&filename, type_query->uri);

	/* Let's make the filename pretty for display & save */
	decode_uri_string_for_display(&filename);

	if (!handler) {
		if (!get_cmd_opt_bool("anonymous")) {
			msg_box(type_query->ses->tab->term, NULL, MSGBOX_FREE_TEXT,
				N_("Unknown type"), ALIGN_CENTER,
				msg_text(type_query->ses->tab->term, N_("Would you like to "
					 "save the file '%s' (type: %s) "
					 "or display it?"),
					 filename.source, ct),
				type_query, 4,
				N_("Sa~ve"), tp_save, B_ENTER,
				N_("~Display"), tp_display, 0,
				N_("Show ~header"), tp_show_header, 0,
				N_("~Cancel"), tp_cancel, B_ESC);
		} else {
			msg_box(type_query->ses->tab->term, NULL, MSGBOX_FREE_TEXT,
				N_("Unknown type"), ALIGN_CENTER,
				msg_text(type_query->ses->tab->term, N_("Would you like to "
					 "display the file '%s' (type: %s)?"),
					 filename.source, ct),
				type_query, 3,
				N_("~Display"), tp_display, B_ENTER,
				N_("Show ~header"), tp_show_header, 0,
				N_("~Cancel"), tp_cancel, B_ESC);
		}
	} else {
		unsigned char *description = handler->description;
		unsigned char *desc_sep = (*description) ? "; " : "";

		if (!get_cmd_opt_bool("anonymous")) {
			/* TODO: Improve the dialog to let the user correct the
			 * used program. */
			msg_box(type_query->ses->tab->term, NULL, MSGBOX_FREE_TEXT,
				N_("What to do?"), ALIGN_CENTER,
				msg_text(type_query->ses->tab->term, N_("Would you like to "
					 "open the file '%s' (type: %s%s%s)\n"
					 "with '%s', save it or display it?"),
					 filename.source, ct, desc_sep,
					 description, handler->program),
				type_query, 5,
				N_("~Open"), tp_open, B_ENTER,
				N_("Sa~ve"), tp_save, 0,
				N_("~Display"), tp_display, 0,
				N_("Show ~header"), tp_show_header, 0,
				N_("~Cancel"), tp_cancel, B_ESC);
		} else {
			msg_box(type_query->ses->tab->term, NULL, MSGBOX_FREE_TEXT,
				N_("What to do?"), ALIGN_CENTER,
				msg_text(type_query->ses->tab->term, N_("Would you like to "
					 "open the file '%s' (type: %s%s%s)\n"
					 "with '%s', or display it?"),
					 filename.source, ct, desc_sep,
					 description, handler->program),
				type_query, 4,
				N_("~Open"), tp_open, B_ENTER,
				N_("~Display"), tp_display, 0,
				N_("Show ~header"), tp_show_header, 0,
				N_("~Cancel"), tp_cancel, B_ESC);
		}
	}

	done_string(&filename);
}

struct {
	unsigned char *type;
	unsigned int plain:1;
} static known_types[] = {
	{ "text/html",			0 },
	{ "application/xhtml+xml",	0 }, /* RFC 3236 */
	{ "text/plain",			1 },
	{ NULL,				1 },
};

int
ses_chktype(struct session *ses, struct download *loading, struct cache_entry *cached, int frame)
{
	struct mime_handler *handler;
	struct view_state *vs;
	struct type_query *type_query;
	unsigned char *ctype = get_content_type(cached);
	int plaintext = 1;
	int ret = 0;
	int xwin, i;

	if (!ctype || !*ctype)
		goto plaintext_follow;

	for (i = 0; known_types[i].type; i++) {
		if (strcasecmp(ctype, known_types[i].type))
			continue;

		plaintext = known_types[i].plain;
		goto plaintext_follow;
	}

	xwin = ses->tab->term->environment & ENV_XWIN;
	handler = get_mime_type_handler(ctype, xwin);

	if (!handler && strlen(ctype) >= 4 && !strncasecmp(ctype, "text", 4))
		goto plaintext_follow;

	type_query = init_type_query(ses, loading, cached);
	if (type_query) {
		ret = 1;
		do_type_query(type_query, ctype, handler);
	}

	mem_free_if(handler);

	return ret;

plaintext_follow:
	vs = ses_forward(ses, frame);
	if (vs) vs->plain = plaintext;
	return 0;
}
