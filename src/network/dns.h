#ifndef EL__NETWORK_DNS_H
#define EL__NETWORK_DNS_H

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

enum dns_result {
	DNS_ERROR	= -1,	/* DNS lookup failed. */
	DNS_SUCCESS	=  0,	/* DNS lookup was successful. */
	DNS_ASYNC	=  1,	/* An async lookup was started. */
};

typedef void (*dns_callback_T)(void *, struct sockaddr_storage *, int);

/* Look up the specified @host using synchronious querying. An array of found
 * addresses will be allocated in @addr with the array length stored in
 * @addrlen. The boolean @called_from_thread is a hack used internally to get
 * the correct allocation method. */
/* Returns non-zero on error and zero on success. */
enum dns_result
do_real_lookup(unsigned char *host, struct sockaddr_storage **addr, int *addrlen,
	       int called_from_thread);

/* Look up the specified @host storing private query information in struct
 * pointed to by @queryref. When the query is done the @done callback will be
 * called with @data, an array of found addresses (or NULL on error) and the
 * address array length. If the boolean @no_cache is non-zero cached DNS queries
 * are ignored. */
/* Returns whether the query is asynchronious. */
enum dns_result find_host(unsigned char *name, void **queryref,
			  dns_callback_T done, void *data, int no_cache);

/* Stop the DNS request pointed to by the @queryref reference. */
void kill_dns_request(void **queryref);

/* Manage the cache of DNS lookups. If the boolean @whole is non-zero all DNS
 * cache entries will be removed. */
void shrink_dns_cache(int whole);

#endif
