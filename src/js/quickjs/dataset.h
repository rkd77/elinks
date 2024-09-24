#ifndef EL__ECMASCRIPT_QUICKJS_DATASET_H
#define EL__ECMASCRIPT_QUICKJS_DATASET_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

JSValue getDataset(JSContext *ctx, void *node);

#ifdef __cplusplus
}
#endif

#endif
