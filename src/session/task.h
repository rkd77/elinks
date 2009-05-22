#ifndef EL__SESSION_TASK_H
#define EL__SESSION_TASK_H

#include "cache/cache.h"
#include "session/session.h"

struct download;
struct location;
struct terminal;
struct view_state;
struct uri;

/** This is for map_selected(), it is used to pass around information
 * about in-imagemap links. */
struct link_def {
	unsigned char *link;
	unsigned char *target;
};

void abort_preloading(struct session *, int);

void ses_load(struct session *ses, struct uri *uri, unsigned char *target_frame,
              struct location *target_location, enum cache_mode cache_mode,
              enum task_type task_type);

void ses_goto(struct session *, struct uri *, unsigned char *,
	      struct location *, enum cache_mode, enum task_type, int);
struct view_state *ses_forward(struct session *, int);

struct uri *get_hooked_uri(unsigned char *uristring, struct session *ses, unsigned char *cwd);

void goto_uri(struct session *ses, struct uri *uri);
void goto_uri_frame(struct session *, struct uri *, unsigned char *, enum cache_mode);
void delayed_goto_uri_frame(void *);
void goto_url(struct session *, unsigned char *);
void goto_url_with_hook(struct session *, unsigned char *);
int goto_url_home(struct session *ses);
void goto_imgmap(struct session *, struct uri *, unsigned char *);
void map_selected(struct terminal *term, void *ld, void *ses);

#endif
