#ifndef EL__DOCUMENT_ECMASCRIPT_LIBDOM_QUICKJS_MAPA_H
#define EL__DOCUMENT_ECMASCRIPT_LIBDOM_QUICKJS_MAPA_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

void attr_save_in_map(void *m, void *node, JSValueConst value);
void *attr_create_new_attrs_map(void);
void *attr_create_new_attributes_map(void);
void *attr_create_new_attributes_map_rev(void);

void attr_clear_map(void *m);
JSValue attr_find_in_map(void *m, void *node);
void attr_erase_from_map(void *m, void *node);

void attr_save_in_map_rev(void *m, JSValueConst value, void *node);
void attr_clear_map_rev(void *m);
void *attr_find_in_map_rev(void *m, JSValueConst value);
void attr_erase_from_map_rev(void *m, JSValueConst value);

#ifdef __cplusplus
}
#endif

#endif
