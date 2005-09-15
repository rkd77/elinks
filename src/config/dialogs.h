/* $Id: dialogs.h,v 1.20 2004/07/14 14:33:46 jonas Exp $ */

#ifndef EL__CONFIG_DIALOGS_H
#define EL__CONFIG_DIALOGS_H

#include "bfu/hierbox.h"
#include "config/kbdbind.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

extern struct hierbox_browser option_browser;
extern struct hierbox_browser keybinding_browser;

void write_config_dialog(struct terminal *term, unsigned char *config_file,
			 int secsave_error, int stdio_error);
void options_manager(struct session *);
void keybinding_manager(struct session *);

struct listbox_item *get_keybinding_action_box_item(enum keymap km, int action);
void init_keybinding_listboxes(struct strtonum *keymaps, struct strtonum *actions[]);
void done_keybinding_listboxes(void);

#endif
