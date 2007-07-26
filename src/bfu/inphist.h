#ifndef EL__BFU_INPHIST_H
#define EL__BFU_INPHIST_H

#include "util/lists.h"

struct dialog_data;


struct input_history_entry {
	LIST_HEAD(struct input_history_entry);
	unsigned char data[1]; /* Must be last. */
};

struct input_history {
	LIST_OF(struct input_history_entry) entries;
	int size;
	unsigned int dirty:1;
	unsigned int nosave:1;
};

#define INIT_INPUT_HISTORY(history)				\
	struct input_history history = {			\
	/* items: */	{ D_LIST_HEAD(history.entries) },	\
	/* size: */	0,					\
	/* dirty: */	0,					\
	/* nosave: */	0,					\
	}

#define add_to_history_list(history, entry)			\
	do {							\
		add_to_list((history)->entries, entry);		\
		(history)->size++;				\
		if (!(history)->nosave)	(history)->dirty = 1;	\
	} while (0)

#define del_from_history_list(history, entry)			\
	do {							\
		del_from_list((entry));				\
		(history)->size--;				\
		if (!(history)->nosave)	(history)->dirty = 1;	\
	} while (0)

void add_to_input_history(struct input_history *, unsigned char *, int);

void do_tab_compl(struct dialog_data *,
		  LIST_OF(struct input_history_entry) *);
void do_tab_compl_file(struct dialog_data *,
		       LIST_OF(struct input_history_entry) *);
void do_tab_compl_unambiguous(struct dialog_data *,
			      LIST_OF(struct input_history_entry) *);

/* Load history file from elinks home. */
int load_input_history(struct input_history *history, unsigned char *filename);

/* Write history list to @filebane in elinks home. It returns a value different
 * from 0 in case of failure, 0 on success. */
int save_input_history(struct input_history *history, unsigned char *filename);

void dlg_set_history(struct widget_data *);

#endif
