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

	/** How many widgets have been added to this dialog.
	 * The #widgets array may be larger than this if some
	 * of the intended widgets have not yet been added.  */
	int number_of_widgets;

	struct widget widgets[1]; /* must be at end of struct */

	/* There may be additional data after widgets[].  The size and
	 * format of the additional data is specific to each dialog.
	 * Dialogs typically place the buffer of each WIDGET_FIELD
	 * widget somewhere in this additional-data area so that the
	 * buffer need not be allocated and freed separately.
	 *
	 * When constructing a dialog, give the size of the additional
	 * data as a parameter to sizeof_dialog() and calloc_dialog().
	 * Then, use get_dialog_offset() to get the address of the
	 * additional data.  */
};

/** Gets the amount of memory needed for a dialog.
 *
 * @param n
 *   How many widgets there will be in the dialog.
 * @param add_size
 *   The size of the additional data, in bytes.
 *
 * @return
 *   The total amount of memory needed for struct dialog, the widgets,
 *   and the additional data.  This however does not include struct
 *   dialog_data.
 *
 * struct dialog already reserves memory for one widget.  */
#define sizeof_dialog(n, add_size) \
	(sizeof(struct dialog) + ((n) - 1) * sizeof(struct widget) + (add_size))

/** Allocates and clears memory for a dialog.
 *
 * @param n
 *   How many widgets there will be in the dialog.
 * @param add_size
 *   The size of the additional data, in bytes.
 *
 * @return
 *   struct dialog *; or NULL if out of memory.
 *
 * This macro sets dialog.number_of_widgets = 0.
 * The caller can then add widgets to the dialog
 * until dialog.number_of_widgets reaches @a n.  */
#define calloc_dialog(n, add_size) ((struct dialog *) mem_calloc(1, sizeof_dialog(n, add_size)))

/** Gets the address of the additional data of a dialog.
 *
 * @param dlg
 *  struct dialog *dlg; the dialog that carries the additional data. 
 * @param n
 *  For how many widgets the dialog was allocated; i.e. the
 *  @c n parameter of sizeof_dialog() and calloc_dialog().
 *  This macro does not read dialog.number_of_widgets because
 *  that is typically still zero when this macro is used.
 *
 * @return
 *  The address of the additional data.  */
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

	/** Size and location of the dialog box, excluding the drop shadow.
	 * This includes the outer border and the frame.
	 * The coordinates are relative to the terminal.  */
	struct box box;

	/** Size and location of the widget area and the inner border.
	 * This is the area in which widgets can be drawn.
	 * The frame of the dialog box is drawn around this area,
	 * and the outer border is around the frame.
	 * The coordinates are relative to the terminal.  */
	struct box real_box;

	/** Vertical scrolling of the widget area of the dialog box.
	 * Widget Y screen coordinate = widget_data.box.y - dialog_data.y.
	 * Initially, this is 0 and the coordinate system of the widget area
	 * matches the coordinate system of the terminal.
	 * Horizontal scrolling of dialog boxes has not been implemented.  */
	int y;

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
