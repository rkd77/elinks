/* SSL socket workshop */
/* $Id: connect.c,v 1.86 2004/11/04 21:10:10 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_OPENSSL
#include <openssl/ssl.h>
#elif defined(CONFIG_GNUTLS)
#include <gnutls/gnutls.h>
#include <gnutls/compat4.h> /* FIXME: this should be removed after upgrading GNUTLS code! */
#else
#error "Huh?! You have SSL enabled, but not OPENSSL nor GNUTLS!! And then you want exactly *what* from me?"
#endif

#include <errno.h>

#include "elinks.h"

#include "config/options.h"
#include "lowlevel/connect.h"
#include "lowlevel/dns.h"
#include "lowlevel/select.h"
#include "sched/connection.h"
#include "ssl/connect.h"
#include "ssl/ssl.h"
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
#define ssl_do_read(socket, rb)		SSL_read(socket->ssl, rb->data + rb->len, rb->freespace)
#define ssl_do_close(socket)		/* Hmh? No idea.. */

#elif defined(CONFIG_GNUTLS)

#define ssl_do_connect(conn)		gnutls_handshake(*((ssl_t *) socket->ssl))
#define ssl_do_write(socket, data, len)	gnutls_record_send(*((ssl_t *) socket->ssl), data, len)
#define ssl_do_read(socket, rb)		gnutls_record_recv(*((ssl_t *) socket->ssl), rb->data + rb->len, rb->freespace)
/* We probably don't handle this entirely correctly.. */
#define ssl_do_close(socket)		gnutls_bye(*((ssl_t *) socket->ssl), GNUTLS_SHUT_RDWR);

#endif


static void
ssl_set_no_tls(struct connection_socket *socket)
{
#ifdef CONFIG_OPENSSL
	((ssl_t *) socket->ssl)->options |= SSL_OP_NO_TLSv1;
#elif defined(CONFIG_GNUTLS)
	/* We do a little more work here, setting up all these priorities (like
	 * they couldn't have some reasonable defaults there).. */

	{
		int protocol_priority[3] = {
			GNUTLS_TLS1,
			GNUTLS_SSL3,
			0
		};

		gnutls_protocol_set_priority(*((ssl_t *) socket->ssl), protocol_priority);
	}

	/* Note that I have no clue about these; I just put all I found here
	 * ;-). It is all a bit confusing for me, and I just want this to work.
	 * Feel free to send me patch removing useless superfluous bloat,
	 * thanks in advance. --pasky */

	{
		int cipher_priority[5] = {
			GNUTLS_CIPHER_RIJNDAEL_128_CBC,
			GNUTLS_CIPHER_3DES_CBC,
			GNUTLS_CIPHER_ARCFOUR,
			GNUTLS_CIPHER_RIJNDAEL_256_CBC,
			0
		};

		gnutls_cipher_set_priority(*((ssl_t *) socket->ssl), cipher_priority);
	}

	{
		/* Does any httpd support this..? ;) */
		int comp_priority[3] = {
			GNUTLS_COMP_ZLIB,
			GNUTLS_COMP_NULL,
			0
		};

		gnutls_compression_set_priority(*((ssl_t *) socket->ssl), comp_priority);
	}

	{
		int kx_priority[5] = {
			GNUTLS_KX_RSA,
			GNUTLS_KX_DHE_DSS,
			GNUTLS_KX_DHE_RSA,
			/* Looks like we don't want SRP, do we? */
			GNUTLS_KX_ANON_DH,
			0
		};

		gnutls_kx_set_priority(*((ssl_t *) socket->ssl), kx_priority);
	}

	{
		int mac_priority[3] = {
			GNUTLS_MAC_SHA,
			GNUTLS_MAC_MD5,
			0
		};

		gnutls_mac_set_priority(*((ssl_t *) socket->ssl), mac_priority);
	}

	{
		int cert_type_priority[2] = {
			GNUTLS_CRT_X509,
			/* We don't link with -extra now; by time of writing
			 * this, it's unclear where OpenPGP will end up. */
			0
		};

		gnutls_cert_type_set_priority(*((ssl_t *) socket->ssl), cert_type_priority);
	}

	gnutls_dh_set_prime_bits(*((ssl_t *) socket->ssl), 1024);
#endif
}

static void
ssl_want_read(struct connection *conn)
{
	struct conn_info *conn_info = conn->conn_info;
	struct connection_socket *socket;

	if (!conn_info) return;

	socket = conn_info->socket;

	if (conn->no_tls)
		ssl_set_no_tls(socket);

	switch (ssl_do_connect(socket)) {
		case SSL_ERROR_NONE:
#ifdef CONFIG_GNUTLS
			if (get_opt_bool("connection.ssl.cert_verify")
			    && gnutls_certificate_verify_peers(*((ssl_t *) socket->ssl))) {
				retry_conn_with_state(conn, S_SSL_ERROR);
				return;
			}
#endif

			done_connection_info(conn);

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_READ2:
			break;

		default:
			conn->no_tls = 1;
			retry_conn_with_state(conn, S_SSL_ERROR);
	}
}

/* Return -1 on error, 0 or success. */
int
ssl_connect(struct connection *conn, struct connection_socket *socket)
{
	int ret;

	if (init_ssl_connection(&conn->socket) == S_SSL_ERROR) {
		abort_conn_with_state(conn, S_SSL_ERROR);
		return -1;
	}

	if (conn->no_tls)
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
	gnutls_transport_set_ptr(*((ssl_t *) socket->ssl),
				 (gnutls_transport_ptr) socket->fd);

	/* TODO: Some certificates fuss. --pasky */
#endif

	ret = ssl_do_connect(socket);

	switch (ret) {
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_READ2:
			set_connection_state(conn, S_SSL_NEG);
			set_handlers(socket->fd, (void (*)(void *)) ssl_want_read,
				     NULL, dns_exception, conn);
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
				conn->no_tls = 1;
			}

			set_connection_state(conn, S_SSL_ERROR);
			close_socket(NULL, conn->conn_info->socket);
			dns_found(conn, 0);
			return -1;
	}

	return 0;
}

/* Return -1 on error, wr or success. */
int
ssl_write(struct connection *conn, struct connection_socket *socket,
	  unsigned char *data, int len)
{
	int wr = ssl_do_write(socket, data, len);

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

		set_connection_state(conn, wr ? (err == SSL_ERROR_SYSCALL ? -errno
							       : S_SSL_ERROR)
				   : S_CANT_WRITE);

		if (!wr || err == SSL_ERROR_SYSCALL)
			retry_connection(conn);
		else
			abort_connection(conn);

		return -1;
	}

	return wr;
}

/* Return -1 on error, rd or success. */
int
ssl_read(struct connection *conn, struct connection_socket *socket,
	 struct read_buffer *rb)
{
	int rd = ssl_do_read(socket, rb);

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
			read_from_socket(conn, rb->socket, rb, rb->done);
			return -1;
		}

		if (rb->close != READ_BUFFER_RETRY_ONCLOSE && !rd) {
			rb->close = READ_BUFFER_END;
			rb->done(conn, rb);
			return -1;
		}

		set_connection_state(conn, rd ? (err == SSL_ERROR_SYSCALL2 ? -errno
								: S_SSL_ERROR)
				   : S_CANT_READ);

		/* mem_free(rb); */

		if (!rd || err == SSL_ERROR_SYSCALL2)
			retry_connection(conn);
		else
			abort_connection(conn);

		return -1;
	}

	return rd;
}

int
ssl_close(struct connection *conn, struct connection_socket *socket)
{
	ssl_do_close(socket);
	done_ssl_connection(socket);

	return 0;
}
