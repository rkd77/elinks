/* SSL socket workshop */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_OPENSSL
#include <openssl/ssl.h>
#elif defined(CONFIG_GNUTLS)
#include <gnutls/gnutls.h>
#else
#error "Huh?! You have SSL enabled, but not OPENSSL nor GNUTLS!! And then you want exactly *what* from me?"
#endif

#include <errno.h>

#include "elinks.h"

#include "config/options.h"
#include "main/select.h"
#include "network/connection.h"
#include "network/socket.h"
#include "network/ssl/socket.h"
#include "network/ssl/ssl.h"
#include "util/memory.h"


/* SSL errors */
#ifdef CONFIG_OPENSSL
#define	SSL_ERROR_WANT_READ2	9999 /* XXX */
#define	SSL_ERROR_WANT_WRITE2	SSL_ERROR_WANT_WRITE
#define	SSL_ERROR_SYSCALL2	SSL_ERROR_SYSCALL
#elif defined(CONFIG_GNUTLS)
#define	SSL_ERROR_NONE		GNUTLS_E_SUCCESS
#define	SSL_ERROR_WANT_READ	GNUTLS_E_AGAIN
#define	SSL_ERROR_WANT_READ2	GNUTLS_E_INTERRUPTED
#define	SSL_ERROR_WANT_WRITE	GNUTLS_E_AGAIN
#define	SSL_ERROR_WANT_WRITE2	GNUTLS_E_INTERRUPTED
#define	SSL_ERROR_SYSCALL	GNUTLS_E_PUSH_ERROR
#define	SSL_ERROR_SYSCALL2	GNUTLS_E_PULL_ERROR
#endif

#ifdef CONFIG_OPENSSL

#define ssl_do_connect(socket)		SSL_get_error(socket->ssl, SSL_connect(socket->ssl))
#define ssl_do_write(socket, data, len)	SSL_write(socket->ssl, data, len)
#define ssl_do_read(socket, data, len)	SSL_read(socket->ssl, data, len)
#define ssl_do_close(socket)		/* Hmh? No idea.. */

#elif defined(CONFIG_GNUTLS)

#define ssl_do_connect(conn)		gnutls_handshake(*((ssl_t *) socket->ssl))
#define ssl_do_write(socket, data, len)	gnutls_record_send(*((ssl_t *) socket->ssl), data, len)
#define ssl_do_read(socket, data, len)	gnutls_record_recv(*((ssl_t *) socket->ssl), data, len)
/* We probably don't handle this entirely correctly.. */
#define ssl_do_close(socket)		gnutls_bye(*((ssl_t *) socket->ssl), GNUTLS_SHUT_RDWR);

#endif


/* Refuse to negotiate TLS 1.0 and later protocols on @socket->ssl.
 * Without this, connecting to <https://www-s.uiuc.edu/> with GnuTLS
 * 1.3.5 would result in an SSL error.  The bug may be in the server
 * (Netscape-Enterprise/3.6 SP3), in GnuTLS, or in ELinks; please log
 * your findings to ELinks bug 712.  */
static void
ssl_set_no_tls(struct socket *socket)
{
#ifdef CONFIG_OPENSSL
	((ssl_t *) socket->ssl)->options |= SSL_OP_NO_TLSv1;
#elif defined(CONFIG_GNUTLS)
	{
		/* GnuTLS does not support SSLv2 because it is "insecure".
		 * That leaves only SSLv3.  */
		static const int protocol_priority[] = {
			GNUTLS_SSL3,
			0
		};

		gnutls_protocol_set_priority(*(ssl_t *) socket->ssl, protocol_priority);
	}
#endif
}

static void
ssl_want_read(struct socket *socket)
{
	if (socket->no_tls)
		ssl_set_no_tls(socket);

	switch (ssl_do_connect(socket)) {
		case SSL_ERROR_NONE:
#ifdef CONFIG_GNUTLS
			if (get_opt_bool("connection.ssl.cert_verify")
			    && gnutls_certificate_verify_peers(*((ssl_t *) socket->ssl))) {
				socket->ops->retry(socket, connection_state(S_SSL_ERROR));
				return;
			}
#endif

			/* Report successful SSL connection setup. */
			complete_connect_socket(socket, NULL, NULL);
			break;

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_READ2:
			break;

		default:
			socket->no_tls = !socket->no_tls;
			socket->ops->retry(socket, connection_state(S_SSL_ERROR));
	}
}

/* Return -1 on error, 0 or success. */
int
ssl_connect(struct socket *socket)
{
	int ret;

	if (init_ssl_connection(socket) == S_SSL_ERROR) {
		socket->ops->done(socket, connection_state(S_SSL_ERROR));
		return -1;
	}

	if (socket->no_tls)
		ssl_set_no_tls(socket);

#ifdef CONFIG_OPENSSL
	SSL_set_fd(socket->ssl, socket->fd);

	if (get_opt_bool("connection.ssl.cert_verify"))
		SSL_set_verify(socket->ssl, SSL_VERIFY_PEER
					  | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
				NULL);

	if (get_opt_bool("connection.ssl.client_cert.enable")) {
		unsigned char *client_cert;

		client_cert = get_opt_str("connection.ssl.client_cert.file");
		if (!*client_cert) {
			client_cert = getenv("X509_CLIENT_CERT");
			if (client_cert && !*client_cert)
				client_cert = NULL;
		}

		if (client_cert) {
			SSL_CTX *ctx = ((SSL *) socket->ssl)->ctx;

			SSL_CTX_use_certificate_chain_file(ctx, client_cert);
			SSL_CTX_use_PrivateKey_file(ctx, client_cert,
						    SSL_FILETYPE_PEM);
		}
	}

#elif defined(CONFIG_GNUTLS)
	/* GnuTLS uses function pointers for network I/O.  The default
	 * functions take a file descriptor, but it must be passed in
	 * as a pointer.  GnuTLS uses the GNUTLS_INT_TO_POINTER and
	 * GNUTLS_POINTER_TO_INT macros for these conversions, but
	 * those are unfortunately not in any public header.  So
	 * ELinks must just cast the pointer the best it can and hope
	 * that the conversions match.  */
	gnutls_transport_set_ptr(*((ssl_t *) socket->ssl),
				 (gnutls_transport_ptr) (longptr_T) socket->fd);

	/* TODO: Some certificates fuss. --pasky */
#endif

	ret = ssl_do_connect(socket);

	switch (ret) {
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_READ2:
			socket->ops->set_state(socket, connection_state(S_SSL_NEG));
			set_handlers(socket->fd, (select_handler_T) ssl_want_read,
				     NULL, (select_handler_T) dns_exception, socket);
			return -1;

		case SSL_ERROR_NONE:
#ifdef CONFIG_GNUTLS
			if (!get_opt_bool("connection.ssl.cert_verify"))
				break;

			if (!gnutls_certificate_verify_peers(*((ssl_t *) socket->ssl)))
#endif
				break;

		default:
			if (ret != SSL_ERROR_NONE) {
				/* DBG("sslerr %s", gnutls_strerror(ret)); */
				socket->no_tls = !socket->no_tls;
			}

			connect_socket(socket, connection_state(S_SSL_ERROR));
			return -1;
	}

	return 0;
}

/* Return enum socket_error on error, bytes written on success. */
ssize_t
ssl_write(struct socket *socket, unsigned char *data, int len)
{
	ssize_t wr = ssl_do_write(socket, data, len);

	if (wr <= 0) {
#ifdef CONFIG_OPENSSL
		int err = SSL_get_error(socket->ssl, wr);
#elif defined(CONFIG_GNUTLS)
		int err = wr;
#endif
		if (err == SSL_ERROR_WANT_WRITE ||
		    err == SSL_ERROR_WANT_WRITE2) {
			return -1;
		}

		if (!wr) return SOCKET_CANT_WRITE;

		if (err == SSL_ERROR_SYSCALL)
			return SOCKET_SYSCALL_ERROR;

		errno = S_SSL_ERROR;
		return SOCKET_INTERNAL_ERROR;
	}

	return wr;
}

/* Return enum socket_error on error, bytes read on success. */
ssize_t
ssl_read(struct socket *socket, unsigned char *data, int len)
{
	ssize_t rd = ssl_do_read(socket, data, len);

	if (rd <= 0) {
#ifdef CONFIG_OPENSSL
		int err = SSL_get_error(socket->ssl, rd);
#elif defined(CONFIG_GNUTLS)
		int err = rd;
#endif

#ifdef CONFIG_GNUTLS
		if (err == GNUTLS_E_REHANDSHAKE)
			return -1;
#endif

		if (err == SSL_ERROR_WANT_READ ||
		    err == SSL_ERROR_WANT_READ2) {
			return SOCKET_SSL_WANT_READ;
		}

		if (!rd) return SOCKET_CANT_READ;

		if (err == SSL_ERROR_SYSCALL2)
			return SOCKET_SYSCALL_ERROR;

		errno = S_SSL_ERROR;
		return SOCKET_INTERNAL_ERROR;
	}

	return rd;
}

int
ssl_close(struct socket *socket)
{
	ssl_do_close(socket);
	done_ssl_connection(socket);

	return 0;
}
