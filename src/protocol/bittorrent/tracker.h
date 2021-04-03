
#ifndef EL__PROTOCOL_BITTORRENT_TRACKER_H
#define EL__PROTOCOL_BITTORRENT_TRACKER_H

#ifdef __cplusplus
extern "C" {
#endif

struct connection;

/* Once called it will periodically request information from the tracker.
 * However, if necessary, it can be called at any time. This is useful if
 * the peer selection run out of peers. */
void send_bittorrent_tracker_request(struct connection *conn);

/* Stops all tracker related activity. */
void done_bittorrent_tracker_connection(struct connection *conn);

#ifdef __cplusplus
}
#endif

#endif
