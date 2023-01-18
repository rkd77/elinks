#ifndef EL__DIALOGS_STATUS_H
#define EL__DIALOGS_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

struct download;
struct session;
struct terminal;

void print_screen_status(struct session *);
void print_screen_status_delayed(struct session *);

void update_status(void);

char *
get_download_msg(struct download *download, struct terminal *term,
	         int wide, int full, const char *separator);

#ifdef __cplusplus
}
#endif

#endif
