#ifndef EL__DOCUMENT_ECMASCRIPT_LIBDOM_MUJS_MAPA_H
#define EL__DOCUMENT_ECMASCRIPT_LIBDOM_MUJS_MAPA_H

#ifdef __cplusplus
extern "C" {
#endif

struct mjs_xhr;

extern void *map_attrs;
extern void *map_attributes;
extern void *map_rev_attributes;
extern void *map_collections;
extern void *map_rev_collections;
extern void *map_doctypes;
extern void *map_elements;
extern void *map_privates;
extern void *map_form_elements;
extern void *map_elements_form;
extern void *map_form;
extern void *map_rev_form;
extern void *map_forms;
extern void *map_rev_forms;
extern void *map_inputs;
extern void *map_nodelist;
extern void *map_rev_nodelist;

void attr_save_in_map(void *m, void *node, void *value);

void *attr_create_new_attrs_map(void);
void *attr_create_new_map(void);

void *attr_find_in_map(void *m, void *node);

void attr_erase_from_map(void *m, void *node);

void attr_clear_map(void *m);
void attr_clear_map_str(void *m);
void delete_map_str(void *m);
void attr_delete_map(void *m);

void *attr_create_new_requestHeaders_map(void);
void *attr_create_new_responseHeaders_map(void);

void process_xhr_headers(char *head, struct mjs_xhr *x);
void set_xhr_header(char *normalized_value, const char *h_name, struct mjs_xhr *x);
const char *get_output_headers(struct mjs_xhr *x);
const char *get_output_header(const char *header_name, struct mjs_xhr *x);

#ifdef __cplusplus
}
#endif

#endif
