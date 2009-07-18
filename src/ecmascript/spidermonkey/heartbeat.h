#ifndef EL__ECMASCRIPT_SPIDERMONKEY_HEARTBEAT_H
#define EL__ECMASCRIPT_SPIDERMONKEY_HEARTBEAT_H

#include "ecmascript/spidermonkey/util.h"

#include "ecmascript/spidermonkey.h"

struct heartbeat {
        LIST_HEAD(struct heartbeat);

        int ttl; /* Time to live.  This value is assigned when the
                  * script begins execution and is decremented every
                  * second.  When it reaches 0, script execution is
                  * terminated. */

        struct ecmascript_interpreter *interpreter;
};

struct heartbeat *add_heartbeat(struct ecmascript_interpreter *interpreter);
void done_heartbeat(struct heartbeat *hb);

JSBool heartbeat_callback(JSContext *ctx);

#endif
