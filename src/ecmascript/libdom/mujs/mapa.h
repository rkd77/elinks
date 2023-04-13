#ifndef EL__DOCUMENT_ECMASCRIPT_LIBDOM_MUJS_MAPA_H
#define EL__DOCUMENT_ECMASCRIPT_LIBDOM_MUJS_MAPA_H

#ifdef __cplusplus
extern "C" {
#endif

struct mjs_xhr;

extern void *map_attrs;

void attr_save_in_map(void *m, void *node, void *value);

void *attr_create_new_attrs_map(void);

void *attr_find_in_map(void *m, void *node);

void attr_erase_from_map(void *m, void *node);

void attr_clear_map(void *m);
void attr_clear_map_str(void *m);
void delete_map_str(void *m);

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
