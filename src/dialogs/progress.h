#ifndef EL__DIALOGS_PROGRESS_H
#define EL__DIALOGS_PROGRESS_H

#ifdef __cplusplus
extern "C" {
#endif

struct progress;
struct terminal;

char *
get_progress_msg(struct progress *progress, struct terminal *term,
		 int wide, int full, const char *separator);


char *
get_upload_progress_msg(struct progress *progress, struct terminal *term,
		 int wide, int full, const char *separator);

/* Draws a progress bar meter or progress coloured text depending on whether
 * @text is NULL. If @meter_color is NULL dialog.meter color is used. */
void
draw_progress_bar(struct progress *progress, struct terminal *term,
		  int x, int y, int width,
		  char *text, struct color_pair *meter_color);

#ifdef __cplusplus
}
#endif

#endif
