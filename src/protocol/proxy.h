
#ifndef EL__PROTOCOL_PROXY_H
#define EL__PROTOCOL_PROXY_H

struct uri;

/* Checks if the passed URI has been configured to go through a proxy. The
 * fragment is removed from the returned URI. */
/* If @connection_state is non-NULL it will be set to indicate what error
 * occured if the function returns NULL. */
struct uri *get_proxy_uri(struct uri *uri, int *connection_state);

/* ``Translates'' the passed URI into the URI being proxied. If it is not a
 * proxy:// URI it will return the URI with the fragment removed.  */
struct uri *get_proxied_uri(struct uri *uri);

#endif
