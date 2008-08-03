
#ifndef EL__PROTOCOL_BITTORRENT_BITTORRENT_H
#define EL__PROTOCOL_BITTORRENT_BITTORRENT_H

#include "main/module.h"
#include "network/state.h"

struct string;
struct uri;

extern struct module bittorrent_protocol_module;

uint32_t get_bittorrent_peerwire_max_message_length(void);
uint32_t get_bittorrent_peerwire_max_request_length(void);

int *get_bittorrent_selection(struct uri *uri, size_t size);
void add_bittorrent_selection(struct uri *uri, int *selection, size_t size);

void add_bittorrent_message(struct uri *uri, struct connection_state state,
			    struct string *);

#endif
