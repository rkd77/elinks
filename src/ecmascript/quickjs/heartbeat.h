#ifndef EL__ECMASCRIPT_QUICKJS_HEARTBEAT_H
#define EL__ECMASCRIPT_QUICKJS_HEARTBEAT_H

#include <quickjs/quickjs.h>

#ifdef __cplusplus
extern "C" {
#endif

struct heartbeat {
        LIST_HEAD_EL(struct heartbeat);

        int ttl; /* Time to live.  This value is assigned when the
                  * script begins execution and is decremented every
                  * second.  When it reaches 0, script execution is
                  * terminated. */

        int ref_count;
        struct ecmascript_interpreter *interpreter;
};

struct heartbeat *add_heartbeat(struct ecmascript_interpreter *interpreter);
void check_heartbeats(void *data);
void done_heartbeat(struct heartbeat *hb);
int js_heartbeat_callback(JSRuntime *rt, void *opaque);

#ifdef __cplusplus
}
#endif

#endif
