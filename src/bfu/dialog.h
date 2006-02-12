#ifndef EL__BFU_DIALOG_H
#define EL__BFU_DIALOG_H

#include "bfu/style.h"
#include "bfu/widget.h"
#include "main/timer.h" /* timer_id_T */
#include "terminal/terminal.h"
#include "terminal/window.h" /* dialog_data->win->term is so common that... */
#include "util/memlist.h"


struct dialog_data;
struct term_event;

struct dialog_layout {
	/* Whether to adjust the dialog width to the maximal width. If not set
	 * only use required width. */
	unsigned int maximize_width:1;
	/* Whether to leave one blank line at the top of the dialog. */
	unsigned int padding_top:1;
	/* Whether to adjust width to fit datalen of _first_ widget. */
	unsigned int fit_datalen:1;
	/* Whether to float grouped widgets on one line. */
	unsigned int float_groups:1;
	/* Whether to draw all the fancy frames and backgrounds. */
	unsigned int only_widgets:1;
};

enum dlg_refresh_code {
	/* The dialog should be redrawn and refreshed again */
	REFRESH_DIALOG,
	/* The dialog should be canceled */
	REFRESH_CANCEL,
	/* The dialog should not be redrawn but refreshed again */
	REFRESH_NONE,
	/* The dialog should not be redrawn or refreshed again */
	REFRESH_STOP,
};

typedef enum dlg_refresh_code (*dialog_refresh_handler_T)(struct dialog_data *, void *);

struct dialog_refresh {
	dialog_refresh_handler_T handler;
	void *data;
	timer_id_T timer;
};

struct dialog {
	unsigned char *title;
	void *udata;
	void *udata2;
	struct dialog_refresh *refresh;

	void (*layouter)(struct dialog_data *);
	widget_handler_status_T (*handle_event)(struct dialog_data *);
	void (*abort)(struct dialog_data *);

	struct dialog_layout layout;

	int number_of_widgets;
	struct widget widgets[1]; /* must be at end of struct */
};

/* Allocate a struct dialog for n - 1 widgets, one is already reserved in struct.
 * add_size bytes will be added. */
#define sizeof_dialog(n, add_size) \
	(sizeof(struct dialog) + ((n) - 1) * sizeof(struct widget) + (add_size))

#define calloc_dialog(n, add_size) ((struct dialog *) mem_calloc(1, sizeof_dialog(n, add_size)))

#define get_dialog_offset(dlg, n) \
	(((unsigned char *) dlg) + sizeof_dialog(n, 0))

#define dialog_has_refresh(dlg_data) \
	((dlg_data)->dlg->refresh && (dlg_data)->dlg->refresh->timer != TIMER_ID_UNDEF)

static inline int
dialog_max_width(struct terminal *term)
{
	int width = term->width * 9 / 10 - 2 * DIALOG_LB;

	int_bounds(&width, 1, int_max(term->width - 2 * DIALOG_LB, 1));

	return width;
}

static inline int
dialog_max_height(struct terminal *term)
{
	int height = term->height * 9 / 10 - 2 * DIALOG_TB;

	int_bounds(&height, 1, int_max(term->height - 2 * DIALOG_TB, 1));

	return height;
}

struct dialog_data {
	struct window *win;
	struct dialog *dlg;
	struct memory_list *ml;

	struct box box;
	int number_of_widgets;
	int selected_widget_id;
	struct term_event *term_event;

	struct widget_data widgets_data[1]; /* must be at end of struct */
};


struct dialog_data *do_dialog(struct terminal *, struct dialog *,
			      struct memory_list *);

/* Draws the dialog background and shadow */
void draw_dialog(struct dialog_data *dlg_data, int width, int height);

/* Draws the dialog borders and layouter if @layout is non zerro. Finally all
 * dialog items are redisplayed. */
void redraw_dialog(struct dialog_data *dlg_data, int layout);

widget_handler_status_T ok_dialog(struct dialog_data *, struct widget_data *);
widget_handler_status_T cancel_dialog(struct dialog_data *, struct widget_data *);
widget_handler_status_T clear_dialog(struct dialog_data *, struct widget_data *);
int check_dialog(struct dialog_data *);
int update_dialog_data(struct dialog_data *);
void generic_dialog_layouter(struct dialog_data *dlg_data);
void refresh_dialog(struct dialog_data *, dialog_refresh_handler_T handler, void *data);

#define selected_widget(dlg_data) (&(dlg_data)->widgets_data[(dlg_data)->selected_widget_id])

void select_widget(struct dialog_data *dlg_data, struct widget_data *widget_data);
struct widget_data *select_widget_by_id(struct dialog_data *dlg_data, int i);

#define before_widgets(dlg_data) (&(dlg_data)->widgets_data[-1])
#define end_of_widgets(dlg_data) (&(dlg_data)->widgets_data[(dlg_data)->number_of_widgets])
#define first_widget(dlg_data) (&(dlg_data)->widgets_data[0])
#define last_widget(dlg_data) (&(dlg_data)->widgets_data[(dlg_data)->number_of_widgets - 1])

#define foreach_widget(dlg_data, widget_data) \
	for ((widget_data) = first_widget(dlg_data); \
	     (widget_data) != end_of_widgets(dlg_data); \
	     (widget_data)++)

#define foreach_widget_back(dlg_data, widget_data) \
	for ((widget_data) = last_widget(dlg_data); \
	     (widget_data) != before_widgets(dlg_data); \
	     (widget_data)--)

#define is_selected_widget(dlg_data, widget_data) ((widget_data) == selected_widget(dlg_data))

#define add_dlg_end(dlg, n)						\
	do {								\
		assert(n == (dlg)->number_of_widgets);			\
	} while (0)

#endif
