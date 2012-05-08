/* SSL socket workshop */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_OPENSSL
#include <openssl/ssl.h>
#include <openssl/x509v3.h>
#define USE_OPENSSL
#elif defined(CONFIG_NSS_COMPAT_OSSL)
#include <nss_compat_ossl/nss_compat_ossl.h>
#define USE_OPENSSL
#elif defined(CONFIG_GNUTLS)
#include <gnutls/gnutls.h>
#include <gnutls/x509.h>
#else
#error "Huh?! You have SSL enabled, but not OPENSSL nor GNUTLS!! And then you want exactly *what* from me?"
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <errno.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "main/select.h"
#include "network/connection.h"
#include "network/socket.h"
#include "network/ssl/match-hostname.h"
#include "network/ssl/socket.h"
#include "network/ssl/ssl.h"
#include "protocol/uri.h"
#include "util/memory.h"


/* SSL errors */
#ifdef USE_OPENSSL
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

#ifdef USE_OPENSSL

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
	/* There is another gnutls_priority_set_direct call elsewhere
	 * in ELinks.  If you change the priorities here, please check
	 * whether that one needs to be changed as well.
	 *
	 * GnuTLS 2.12.x is said to support "-VERS-TLS-ALL" too, but
	 * that version hasn't yet been released as of May 2011.  */
	gnutls_priority_set_direct(*(ssl_t *) socket->ssl,
				   "SECURE:-CTYPE-OPENPGP"
				   ":+VERS-SSL3.0:-VERS-TLS1.0"
				   ":-VERS-TLS1.1:-VERS-TLS1.2"
				   ":%SSL3_RECORD_VERSION",
				   NULL);
#endif
}


#ifdef CONFIG_GNUTLS
static int
verify_certificates(struct socket *socket)
{
	gnutls_x509_crt_t cert;
	gnutls_session_t session = *(ssl_t *)socket->ssl;
	struct connection *conn = socket->conn;
	const gnutls_datum_t *cert_list;
	unsigned char *hostname;
	int ret;
	unsigned int cert_list_size, status;


	ret = gnutls_certificate_verify_peers2(session, &status);
	if (ret) return ret;
	if (status) return status;

	/* If the certificate is of a type for which verification has
	 * not yet been implemented, then reject it.  This way, a fake
	 * server cannot avoid verification by using a strange type of
	 * certificate.
	 *
	 * OpenPGP certificates shouldn't even get this far anyway,
	 * because init_ssl_connection() tells GnuTLS to disable
	 * OpenPGP, and ELinks never calls
	 * gnutls_certificate_set_openpgp_keyring_file, so status
	 * should have been GNUTLS_CERT_SIGNER_NOT_FOUND.  */
	if (gnutls_certificate_type_get(session) != GNUTLS_CRT_X509)
		return -7;

	if (gnutls_x509_crt_init(&cert) < 0) {
		return -1;
	}

	cert_list = gnutls_certificate_get_peers(session, &cert_list_size);
	if (!cert_list) {
		return -2;
	}

	if (gnutls_x509_crt_import(cert, &cert_list[0],
		GNUTLS_X509_FMT_DER) < 0) {
		return -3;
	}
	if (gnutls_x509_crt_get_expiration_time(cert) < time(NULL)) {
		gnutls_x509_crt_deinit(cert);
		return -4;
	}

	if (gnutls_x509_crt_get_activation_time(cert) > time(NULL)) {
		gnutls_x509_crt_deinit(cert);
		return -5;
	}

	/* Because RFC 5280 defines dNSName as an IA5String, it can
	 * only contain ASCII characters.  Internationalized domain
	 * names must thus be in Punycode form.  Because GnuTLS 2.8.6
	 * does not itself support IDN, ELinks must convert.  */
	hostname = get_uri_string(conn->uri, URI_HOST | URI_IDN);
	if (!hostname) return -6;

	ret = !gnutls_x509_crt_check_hostname(cert, hostname);
	gnutls_x509_crt_deinit(cert);
	mem_free(hostname);
	return ret;
}
#endif	/* CONFIG_GNUTLS */

#ifdef USE_OPENSSL

/** Checks whether the host component of a URI matches a host name in
 * the server certificate.
 *
 * @param[in] uri_host
 *   The host name (or IP address) to which the user wanted to connect.
 *   Should be in UTF-8.
 * @param[in] cert_host_asn1
 *   A host name found in the server certificate: either as commonName
 *   in the subject field, or as a dNSName in the subjectAltName
 *   extension.  This may contain wildcards, as specified in RFC 2818
 *   section 3.1.
 *
 * @return
 *   Nonzero if the host matches.  Zero if it doesn't, or on error.
 *
 * If @a uri_host is an IP address literal rather than a host name,
 * then this function returns 0, meaning that the host name does not match.
 * According to RFC 2818, if the certificate is intended to match an
 * IP address, then it must have that IP address as an iPAddress
 * SubjectAltName, rather than in commonName.  For comparing those,
 * match_uri_host_ip() must be used instead of this function.  */
static int
match_uri_host_name(const unsigned char *uri_host,
		    ASN1_STRING *cert_host_asn1)
{
	const size_t uri_host_len = strlen(uri_host);
	unsigned char *cert_host = NULL;
	int cert_host_len;
	int matched = 0;

	if (is_ip_address(uri_host, uri_host_len))
		goto mismatch;

	/* This function is used for both dNSName and commonName.
	 * Although dNSName is always an IA5 string, commonName allows
	 * many other encodings too.  Normalize all to UTF-8.  */
	cert_host_len = ASN1_STRING_to_UTF8(&cert_host,
					    cert_host_asn1);
	if (cert_host_len < 0)
		goto mismatch;

	matched = match_hostname_pattern(uri_host, uri_host_len,
					 cert_host, cert_host_len);

mismatch:
	if (cert_host)
		OPENSSL_free(cert_host);
	return matched;
}

/** Checks whether the host component of a URI matches an IP address
 * in the server certificate.
 *
 * @param[in] uri_host
 *   The IP address (or host name) to which the user wanted to connect.
 *   Should be in UTF-8.
 * @param[in] cert_host_asn1
 *   An IP address found as iPAddress in the subjectAltName extension
 *   of the server certificate.  According to RFC 5280 section 4.2.1.6,
 *   that is an octet string in network byte order.  According to
 *   RFC 2818 section 3.1, wildcards are not allowed.
 *
 * @return
 *   Nonzero if the host matches.  Zero if it doesn't, or on error.
 *
 * If @a uri_host is a host name rather than an IP address literal,
 * then this function returns 0, meaning that the address does not match.
 * This function does not try to resolve the host name to an IP address
 * and compare that to @a cert_host_asn1, because such an approach would
 * be vulnerable to DNS spoofing.
 *
 * This function does not support the address-and-netmask format used
 * in the name constraints extension of a CA certificate (RFC 5280
 * section 4.2.1.10).  */
static int
match_uri_host_ip(const unsigned char *uri_host,
		  ASN1_OCTET_STRING *cert_host_asn1)
{
	const unsigned char *cert_host_addr = ASN1_STRING_data(cert_host_asn1);
	struct in_addr uri_host_in;
#ifdef CONFIG_IPV6
	struct in6_addr uri_host_in6;
#endif

	/* RFC 5280 defines the iPAddress alternative of GeneralName
	 * as an OCTET STRING.  Verify that the type is indeed that.
	 * This is an assertion because, if someone puts the wrong
	 * type of data there, then it will not even be recognized as
	 * an iPAddress, and this function will not be called.
	 *
	 * (Because GeneralName is defined in an implicitly tagged
	 * ASN.1 module, the OCTET STRING tag is not part of the DER
	 * encoding.  BER also allows a constructed encoding where
	 * each substring begins with the OCTET STRING tag; but ITU-T
	 * Rec. X.690 (07/2002) subclause 8.21 says those would be
	 * OCTET STRING even if the outer string were of some other
	 * type.  "A Layman's Guide to a Subset of ASN.1, BER, and
	 * DER" (Kaliski, 1993) claims otherwise, though.)  */
	assert(ASN1_STRING_type(cert_host_asn1) == V_ASN1_OCTET_STRING);
	if_assert_failed return 0;

	/* cert_host_addr, url_host_in, and url_host_in6 are all in
	 * network byte order.  */
	switch (ASN1_STRING_length(cert_host_asn1)) {
	case 4:
		return inet_aton(uri_host, &uri_host_in) != 0
		    && memcmp(cert_host_addr, &uri_host_in.s_addr, 4) == 0;

#ifdef CONFIG_IPV6
	case 16:
		return inet_pton(AF_INET6, uri_host, &uri_host_in6) == 1
		    && memcmp(cert_host_addr, &uri_host_in6.s6_addr, 16) == 0;
#endif

	default:
		return 0;
	}
}

/** Verify one certificate in the server certificate chain.
 * This callback is documented in SSL_set_verify(3).  */
static int
verify_callback(int preverify_ok, X509_STORE_CTX *ctx)
{
	X509 *cert;
	SSL *ssl;
	struct socket *socket;
	struct connection *conn;
	unsigned char *host_in_uri;
	GENERAL_NAMES *alts;
	int saw_dns_name = 0;
	int matched = 0;

	/* If OpenSSL already found a problem, keep that.  */
	if (!preverify_ok)
		return 0;

	/* Examine only the server certificate, not CA certificates.  */
	if (X509_STORE_CTX_get_error_depth(ctx) != 0)
		return preverify_ok;

	cert = X509_STORE_CTX_get_current_cert(ctx);
	ssl = X509_STORE_CTX_get_ex_data(ctx, SSL_get_ex_data_X509_STORE_CTX_idx());
	socket = SSL_get_ex_data(ssl, socket_SSL_ex_data_idx);
	conn = socket->conn;
	host_in_uri = get_uri_string(conn->uri, URI_HOST | URI_IDN);
	if (!host_in_uri)
		return 0;

	/* RFC 5280 section 4.2.1.6 describes the subjectAltName extension.
	 * RFC 2818 section 3.1 says Common Name must not be used
	 * if dNSName is present.  */
	alts = X509_get_ext_d2i(cert, NID_subject_alt_name, NULL, NULL);
	if (alts != NULL) {
		int alt_count;
		int alt_pos;
		GENERAL_NAME *alt;

		alt_count = sk_GENERAL_NAME_num(alts);
		for (alt_pos = 0; !matched && alt_pos < alt_count; ++alt_pos) {
			alt = sk_GENERAL_NAME_value(alts, alt_pos);
			if (alt->type == GEN_DNS) {
				saw_dns_name = 1;
				matched = match_uri_host_name(host_in_uri,
							      alt->d.dNSName);
			} else if (alt->type == GEN_IPADD) {
				matched = match_uri_host_ip(host_in_uri,
							    alt->d.iPAddress);
			}
		}

		/* Free the GENERAL_NAMES list and each element.  */
		sk_GENERAL_NAME_pop_free(alts, GENERAL_NAME_free);
	}

	if (!matched && !saw_dns_name) {
		X509_NAME *name;
		int cn_index;
		X509_NAME_ENTRY *entry = NULL;

		name = X509_get_subject_name(cert);
		cn_index = X509_NAME_get_index_by_NID(name, NID_commonName, -1);
		if (cn_index >= 0)
			entry = X509_NAME_get_entry(name, cn_index);
		if (entry != NULL)
			matched = match_uri_host_name(host_in_uri,
						      X509_NAME_ENTRY_get_data(entry));
	}

	mem_free(host_in_uri);
	return matched;
}

#endif	/* USE_OPENSSL */

static void
ssl_want_read(struct socket *socket)
{
	if (socket->no_tls)
		ssl_set_no_tls(socket);

	switch (ssl_do_connect(socket)) {
		case SSL_ERROR_NONE:
#ifdef CONFIG_GNUTLS
			if (get_opt_bool("connection.ssl.cert_verify", NULL)
			    && verify_certificates(socket)) {
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
	unsigned char *server_name;
	struct connection *conn = socket->conn;

	/* TODO: Recode server_name to UTF-8.  */
	server_name = get_uri_string(conn->proxied_uri, URI_HOST);
	if (!server_name) {
		socket->ops->done(socket, connection_state(S_OUT_OF_MEM));
		return -1;
	}

	/* RFC 3546 says literal IPv4 and IPv6 addresses are not allowed.  */
	if (is_ip_address(server_name, strlen(server_name)))
		mem_free_set(&server_name, NULL);

	if (init_ssl_connection(socket, server_name) == S_SSL_ERROR) {
		mem_free_if(server_name);
		socket->ops->done(socket, connection_state(S_SSL_ERROR));
		return -1;
	}

	mem_free_if(server_name);

	if (socket->no_tls)
		ssl_set_no_tls(socket);

#ifdef USE_OPENSSL
	SSL_set_fd(socket->ssl, socket->fd);

	if (get_opt_bool("connection.ssl.cert_verify", NULL))
		SSL_set_verify(socket->ssl, SSL_VERIFY_PEER
					  | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
			       verify_callback);

	if (get_opt_bool("connection.ssl.client_cert.enable", NULL)) {
		unsigned char *client_cert;

#ifdef CONFIG_NSS_COMPAT_OSSL
		client_cert = get_opt_str(
				"connection.ssl.client_cert.nickname", NULL);
#else
		client_cert = get_opt_str(
				"connection.ssl.client_cert.file", NULL);
#endif
		if (!*client_cert) {
			client_cert = getenv("X509_CLIENT_CERT");
			if (client_cert && !*client_cert)
				client_cert = NULL;
		}

		if (client_cert) {
#ifdef CONFIG_NSS_COMPAT_OSSL
			SSL_CTX_use_certificate_chain_file(
					(SSL *) socket->ssl,
					client_cert);
#else
			SSL_CTX *ctx = ((SSL *) socket->ssl)->ctx;

			SSL_CTX_use_certificate_chain_file(ctx, client_cert);
			SSL_CTX_use_PrivateKey_file(ctx, client_cert,
						    SSL_FILETYPE_PEM);
#endif
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
				 (gnutls_transport_ptr_t) (longptr_T) socket->fd);

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
			if (!get_opt_bool("connection.ssl.cert_verify", NULL))
				break;

			if (!verify_certificates(socket))
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
#ifdef USE_OPENSSL
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
#ifdef USE_OPENSSL
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
