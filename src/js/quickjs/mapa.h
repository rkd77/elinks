#ifndef EL__DOCUMENT_ECMASCRIPT_LIBDOM_QUICKJS_MAPA_H
#define EL__DOCUMENT_ECMASCRIPT_LIBDOM_QUICKJS_MAPA_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Xhr;

extern void *map_attrs;
extern void *map_attributes;
extern void *map_rev_attributes;
extern void *map_doctypes;
extern void *map_privates;
extern void *map_form;
extern void *map_form_rev;
extern void *map_forms;
extern void *map_rev_forms;
extern void *map_inputs;
extern void *map_nodelist;
extern void *map_rev_nodelist;
extern void *map_form_elements;
extern void *map_form_elements_rev;

extern void *map_interp;

void attr_save_in_map(void *m, void *node, JSValueConst value);
void attr_save_in_map_void(void *m, void *node, void *value);

void *interp_new_map(void);
bool interp_find_in_map(void *m, void *interpreter);
void interp_save_in_map(void *m, void *interpreter);
void interp_erase_from_map(void *m, void *interpreter);
void interp_delete_map(void *m);

void *attr_create_new_attrs_map(void);
void *attr_create_new_attributes_map(void);
void *attr_create_new_attributes_map_rev(void);
void *attr_create_new_collections_map(void);
void *attr_create_new_collections_map_rev(void);
void *attr_create_new_doctypes_map(void);
void *attr_create_new_elements_map(void);
void *attr_create_new_privates_map_void(void);
void *attr_create_new_form_elements_map(void);
void *attr_create_new_form_elements_map_rev(void);
void *attr_create_new_form_map(void);
void *attr_create_new_form_map_rev(void);
void *attr_create_new_forms_map(void);
void *attr_create_new_forms_map_rev(void);
void *attr_create_new_input_map(void);
void *attr_create_new_nodelist_map(void);
void *attr_create_new_nodelist_map_rev(void);

void *attr_create_new_csses_map(void);
void *attr_create_new_csses_map_rev(void);

void *attr_create_new_requestHeaders_map(void);
void *attr_create_new_responseHeaders_map(void);

//void attr_clear_map(void *m);
//void attr_clear_map_rev(void *m);
//void attr_clear_map_void(void *m);
//void attr_clear_map_str(void *m);
void delete_map_str(void *m);
void attr_delete_map(void *m);
void attr_delete_map_rev(void *m);
void attr_delete_map_void(void *m);

JSValue attr_find_in_map(void *m, void *node);
void *attr_find_in_map_void(void *m, void *node);

void attr_erase_from_map(void *m, void *node);
void attr_erase_from_map_str(void *m, void *node);

void attr_save_in_map_rev(void *m, JSValueConst value, void *node);
//void attr_clear_map_rev(void *m);
void *attr_find_in_map_rev(void *m, JSValueConst value);
void attr_erase_from_map_rev(void *m, JSValueConst value);

void process_xhr_headers(char *head, struct Xhr *x);
void set_xhr_header(char *normalized_value, const char *h_name, struct Xhr *x);
char *get_output_headers(struct Xhr *x);
char *get_output_header(const char *header_name, struct Xhr *x);

char *get_elstyle(void *m);
void *set_elstyle(const char *text);
char *get_css_value(const char *text, const char *param);
char *set_css_value(const char *text, const char *param, const char *value);


#ifdef __cplusplus
}
#endif

#endif
