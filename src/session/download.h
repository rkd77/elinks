#ifndef EL__SESSION_DOWNLOAD_H
#define EL__SESSION_DOWNLOAD_H

#include "main/object.h"
#include "network/state.h"
#include "util/lists.h"
#include "util/time.h"

/* Silly BFU stuff */
struct dialog_data;
struct listbox_item;
struct terminal;

struct cache_entry;
struct connection;
struct session;
struct uri;

struct download;

typedef void (download_callback_T)(struct download *, void *);

struct download {
	/* XXX: order matters there, there's some hard initialization in
	 * src/session/session.c and src/viewer/text/view.c */
	LIST_HEAD(struct download);

	struct connection *conn;
	struct cache_entry *cached;
	/** The callback is called when connection gets into a progress state,
	 * after it's over (in a result state), and also periodically after
	 * the download starts receiving some data. */
	download_callback_T *callback;
	void *data;
	struct progress *progress;

	struct connection_state state;
	struct connection_state prev_error;
	enum connection_priority pri;
};

struct type_query {
	LIST_HEAD(struct type_query);
	struct download download;
	struct cache_entry *cached;
	struct session *ses;
	struct uri *uri;
	unsigned char *target_frame;
	unsigned char *external_handler;
	int block;
	unsigned int cgi:1;
	/* int frame; */
};

struct file_download {
	OBJECT_HEAD(struct file_download);

	struct uri *uri;
	unsigned char *file;
	unsigned char *external_handler;
	struct session *ses;
	struct terminal *term;
	time_t remotetime;
	off_t seek;
	int handle;
	int redirect_cnt;
	int notify;
	struct download download;

	/** Should the file be deleted when destroying the structure */
	unsigned int delete:1;

	/** Should the download be stopped/interrupted when destroying the structure */
	unsigned int stop:1;

	/** Whether to block the terminal when running the external handler. */
	unsigned int block:1;

	/** The current dialog for this download. Can be NULL. */
	struct dialog_data *dlg_data;
	struct listbox_item *box_item;
};

/** Stack of all running downloads */
extern LIST_OF(struct file_download) downloads;

static inline int
is_in_downloads_list(struct file_download *file_download)
{
	struct file_download *down;

	foreach (down, downloads)
		if (file_download == down) return 1;

	return 0;
}

int download_is_progressing(struct download *download);

int are_there_downloads(void);

void start_download(void *, unsigned char *);
void resume_download(void *, unsigned char *);
void create_download_file(struct terminal *, unsigned char *, unsigned char **,
			  int, int,
			  void (*)(struct terminal *, int, void *, int),
			  void *);

void abort_all_downloads(void);
void destroy_downloads(struct session *);
void detach_downloads_from_terminal(struct terminal *);

int setup_download_handler(struct session *, struct download *, struct cache_entry *, int);

void abort_download(struct file_download *file_download);
void done_type_query(struct type_query *type_query);

void tp_display(struct type_query *type_query);
void tp_save(struct type_query *type_query);
void tp_cancel(void *data);
struct file_download *init_file_download(struct uri *uri, struct session *ses, unsigned char *file, int fd);

#endif
