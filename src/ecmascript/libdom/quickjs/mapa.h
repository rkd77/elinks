#ifndef EL__DOCUMENT_ECMASCRIPT_LIBDOM_QUICKJS_MAPA_H
#define EL__DOCUMENT_ECMASCRIPT_LIBDOM_QUICKJS_MAPA_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct Xhr;

void attr_save_in_map(void *m, void *node, JSValueConst value);
void attr_save_in_map_void(void *m, void *node, void *value);

void *attr_create_new_attrs_map(void);
void *attr_create_new_attributes_map(void);
void *attr_create_new_attributes_map_rev(void);
void *attr_create_new_collections_map(void);
void *attr_create_new_collections_map_rev(void);
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

void *attr_create_new_requestHeaders_map(void);
void *attr_create_new_responseHeaders_map(void);

void attr_clear_map(void *m);
void attr_clear_map_str(void *m);
void delete_map_str(void *m);

JSValue attr_find_in_map(void *m, void *node);
void *attr_find_in_map_void(void *m, void *node);

void attr_erase_from_map(void *m, void *node);

void attr_save_in_map_rev(void *m, JSValueConst value, void *node);
void attr_clear_map_rev(void *m);
void *attr_find_in_map_rev(void *m, JSValueConst value);
void attr_erase_from_map_rev(void *m, JSValueConst value);

void process_xhr_headers(char *head, struct Xhr *x);
void set_xhr_header(char *normalized_value, const char *h_name, struct Xhr *x);
const char *get_output_headers(struct Xhr *x);
const char *get_output_header(const char *header_name, struct Xhr *x);


#ifdef __cplusplus
}
#endif

#endif
