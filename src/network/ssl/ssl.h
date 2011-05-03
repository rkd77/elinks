
#ifndef EL__NETWORK_SSL_SSL_H
#define EL__NETWORK_SSL_SSL_H

#ifdef CONFIG_SSL

#include "main/module.h"

struct socket;

extern struct module ssl_module;

/* Initializes the SSL connection data. Returns S_OK on success and S_SSL_ERROR
 * on failure.
 *
 * server_name is the DNS name of the server (in UTF-8), or NULL if
 * ELinks knows only the IP address.  ELinks reports that name to the
 * server so that the server can choose the correct certificate if it
 * has multiple virtual hosts on the same IP address.  See RFC 3546
 * section 3.1.
 *
 * server_name does not affect how ELinks verifies the certificate
 * after the server has returned it.  */
int init_ssl_connection(struct socket *socket,
			const unsigned char *server_name);

/* Releases the SSL connection data */
void done_ssl_connection(struct socket *socket);

unsigned char *get_ssl_connection_cipher(struct socket *socket);

#if defined(CONFIG_OPENSSL) || defined(CONFIG_NSS_COMPAT_OSSL)
extern int socket_SSL_ex_data_idx;
#endif

/* Internal type used in ssl module. */

#if defined(CONFIG_OPENSSL) || defined(CONFIG_NSS_COMPAT_OSSL)
#define	ssl_t	SSL
#elif defined(CONFIG_GNUTLS)
#define	ssl_t	gnutls_session_t
#endif

#endif /* CONFIG_SSL */
#endif
