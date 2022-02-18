
#ifndef EL__BFU_LISTMENU_H
#define EL__BFU_LISTMENU_H

#ifdef __cplusplus
extern "C" {
#endif

struct session;
struct string;
struct terminal;

struct list_menu {
	struct menu_item **stack;
	int stack_size;
};

void init_menu(struct list_menu *menu);
void destroy_menu(struct list_menu *menu);
void add_select_item(struct list_menu *menu, struct string *string, struct string *orig_string, char **value, int order, int dont_add);
void new_menu_item(struct list_menu *menu, char *name, int data, int fullname);
struct menu_item *detach_menu(struct list_menu *menu);
void menu_labels(struct menu_item *m, const char *base, char **lbls);
void do_select_submenu(struct terminal *term, void *menu_, void *ses_);
void free_menu(struct menu_item *m);

extern struct list_menu lnk_menu;

#ifdef __cplusplus
}
#endif

#endif /* EL__BFU_LISTMENU_H */
