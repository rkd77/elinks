#ifndef EL__DOCUMENT_LIBDOM_MAPA_H
#define EL__DOCUMENT_LIBDOM_MAPA_H

#ifdef __cplusplus
extern "C" {
#endif

void save_in_map(void *m, void *node, int length);
void *create_new_element_map(void);
void clear_map(void *m);

#ifdef __cplusplus
}
#endif

#endif
