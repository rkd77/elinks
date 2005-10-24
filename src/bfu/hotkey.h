/* Hotkeys handling. */

#ifndef EL__BFU_HOTKEY_H
#define EL__BFU_HOTKEY_H

struct menu;
struct terminal;

/* int find_hotkey_pos(unsigned char *text); */
void init_hotkeys(struct terminal *term, struct menu *menu);
#ifdef CONFIG_NLS
void clear_hotkeys_cache(struct menu *menu);
#endif
void refresh_hotkeys(struct terminal *term, struct menu *menu);
/* int is_hotkey(struct menu_item *item, unsigned char key, struct terminal *term); */
int check_hotkeys(struct menu *menu, unsigned char hotkey, struct terminal *term);
int check_not_so_hot_keys(struct menu *menu, unsigned char key, struct terminal *term);

#endif
