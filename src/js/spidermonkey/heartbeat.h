#ifndef EL__JS_SPIDERMONKEY_HEARTBEAT_H
#define EL__JS_SPIDERMONKEY_HEARTBEAT_H

#include "js/spidermonkey/util.h"

#include "js/spidermonkey.h"

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
void done_heartbeat(struct heartbeat *hb);

bool heartbeat_callback(JSContext *ctx);

#endif
