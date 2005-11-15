#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "bfu/hierbox.h"
#include "config/kbdbind.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

extern struct hierbox_browser option_browser;
extern struct hierbox_browser keybinding_browser;

void write_config_dialog(struct terminal *term, unsigned char *config_file,
			 int secsave_error, int stdio_error);
void options_manager(struct session *);
void keybinding_manager(struct session *);

struct listbox_item *get_keybinding_action_box_item(enum keymap_id keymap_id, action_id_T action_id);
void init_keybinding_listboxes(struct keymap keymap_table[], struct action_list actions[]);
void done_keybinding_listboxes(void);

#endif
