
#ifndef EL__BFU_LISTMENU_H
#define EL__BFU_LISTMENU_H

struct session;
struct string;
struct terminal;

struct list_menu {
	struct menu_item **stack;
	int stack_size;
};

void init_menu(struct list_menu *menu);
void destroy_menu(struct list_menu *menu);
void add_select_item(struct list_menu *menu, struct string *string, struct string *orig_string, unsigned char **value, int order, int dont_add);
void new_menu_item(struct list_menu *menu, unsigned char *name, int data, int fullname);
struct menu_item *detach_menu(struct list_menu *menu);
void menu_labels(struct menu_item *m, unsigned char *base, unsigned char **lbls);
void do_select_submenu(struct terminal *term, void *menu_, void *ses_);
void free_menu(struct menu_item *m);

#endif /* EL__BFU_LISTMENU_H */
