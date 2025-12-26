#ifndef EL__JS_TIMER_H
#define EL__JS_TIMER_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ecmascript_timeout;

void init_map_timer(void);
void done_map_timer(void);
void add_to_map_timer(struct ecmascript_timeout *t);
void del_from_map_timer(struct ecmascript_timeout *t);
struct ecmascript_timeout *find_in_map_timer(uint32_t id);

#ifdef __cplusplus
}
#endif

#endif
