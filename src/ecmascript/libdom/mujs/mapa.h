#ifndef EL__DOCUMENT_ECMASCRIPT_LIBDOM_MUJS_MAPA_H
#define EL__DOCUMENT_ECMASCRIPT_LIBDOM_MUJS_MAPA_H

#ifdef __cplusplus
extern "C" {
#endif

extern void *map_attrs;

void attr_save_in_map(void *m, void *node, void *value);

void *attr_create_new_attrs_map(void);

void *attr_find_in_map(void *m, void *node);

void attr_erase_from_map(void *m, void *node);

void attr_clear_map(void *m);

#ifdef __cplusplus
}
#endif

#endif
