#ifndef EL__DOCUMENT_LIBDOM_MAPA_H
#define EL__DOCUMENT_LIBDOM_MAPA_H

#ifdef __cplusplus
extern "C" {
#endif

void save_in_map(void *m, void *node, int length);
void save_offset_in_map(void *m, void *node, int offset);
void *create_new_element_map(void);
void *create_new_element_map_rev(void);
void clear_map(void *m);
void delete_map(void *m);
void clear_map_rev(void *m);
void delete_map_rev(void *m);

void *find_in_map(void *m, int offset);
int find_offset(void *m, void *node);

#ifdef __cplusplus
}
#endif

#endif
