#ifndef EL__PROTOCOL_COMMON_H
#define EL__PROTOCOL_COMMON_H

#include "network/state.h"

struct string;
struct uri;

/* Close all non-terminal file descriptors. */
void close_all_non_term_fd(void);

struct connection_state
init_directory_listing(struct string *page, struct uri *uri);

#endif
