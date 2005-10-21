/* SSL support - wrappers for SSL routines */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef CONFIG_OPENSSL
#include <openssl/ssl.h>
#include <openssl/rand.h>
#elif defined(CONFIG_GNUTLS)
#include <gnutls/gnutls.h>
#else
#error "Huh?! You have SSL enabled, but not OPENSSL nor GNUTLS!! And then you want exactly *what* from me?"
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "network/connection.h"
#include "network/socket.h"
#include "network/ssl/ssl.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/string.h"


/* FIXME: As you can see, SSL is currently implemented in very, erm,
 * decentralized manner. */

#ifdef CONFIG_OPENSSL

#ifndef PATH_MAX
#define	PATH_MAX	256 /* according to my /usr/include/bits/posix1_lim.h */
#endif

SSL_CTX *context = NULL;

static void
init_openssl(struct module *module)
{
	unsigned char f_randfile[PATH_MAX];

	/* In a nutshell, on OS's without a /dev/urandom, the OpenSSL library
	 * cannot initialize the PRNG and so every attempt to use SSL fails.
	 * It's actually an OpenSSL FAQ, and according to them, it's up to the
	 * application coders to seed the RNG. -- William Yodlowsky */
	if (RAND_egd(RAND_file_name(f_randfile, sizeof(f_randfile))) < 0) {
		/* Not an EGD, so read and write to it */
		if (RAND_load_file(f_randfile, -1))
			RAND_write_file(f_randfile);
	}

	SSLeay_add_ssl_algorithms();
	context = SSL_CTX_new(SSLv23_client_method());
	SSL_CTX_set_options(context, SSL_OP_ALL);
	SSL_CTX_set_default_verify_paths(context);
}

static void
done_openssl(struct module *module)
{
	if (context) SSL_CTX_free(context);
}

static struct option_info openssl_options[] = {
	INIT_OPT_BOOL("connection.ssl", N_("Verify certificates"),
		"cert_verify", 0, 0,
		N_("Verify the peer's SSL certificate. Note that this\n"
		"needs extensive configuration of OpenSSL by the user.")),

	INIT_OPT_TREE("connection.ssl", N_("Client Certificates"),
        	"client_cert", OPT_SORT,
        	N_("X509 client certificate options.")),

	INIT_OPT_BOOL("connection.ssl.client_cert", N_("Enable"),
		"enable", 0, 0,
		 N_("Enable or not the sending of X509 client certificates\n"
		    "to servers which request them.")),

	INIT_OPT_STRING("connection.ssl.client_cert", N_("Certificate File"),
		"file", 0, "",
		 N_("The location of a file containing the client certificate\n"
		    "and unencrypted private key in PEM format. If unset, the\n"
		    "file pointed to by the X509_CLIENT_CERT variable is used\n"
		    "instead.")),

	NULL_OPTION_INFO,
};

static struct module openssl_module = struct_module(
	/* name: */		"OpenSSL",
	/* options: */		openssl_options,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_openssl,
	/* done: */		done_openssl
);

#elif defined(CONFIG_GNUTLS)

gnutls_anon_client_credentials_t anon_cred = NULL;
gnutls_certificate_credentials_t xcred = NULL;

const static int protocol_priority[16] = {
	GNUTLS_TLS1, GNUTLS_SSL3, 0
};
const static int kx_priority[16] = {
	GNUTLS_KX_RSA, GNUTLS_KX_DHE_DSS, GNUTLS_KX_DHE_RSA, GNUTLS_KX_SRP,
	/* Do not use anonymous authentication, unless you know what that means */
	GNUTLS_KX_ANON_DH, GNUTLS_KX_RSA_EXPORT, 0
};
const static int cipher_priority[16] = {
	GNUTLS_CIPHER_RIJNDAEL_128_CBC, GNUTLS_CIPHER_ARCFOUR_128,
	GNUTLS_CIPHER_3DES_CBC, GNUTLS_CIPHER_AES_256_CBC, GNUTLS_CIPHER_ARCFOUR_40, 0
};
const static int comp_priority[16] = { GNUTLS_COMP_ZLIB, GNUTLS_COMP_NULL, 0 };
const static int mac_priority[16] = { GNUTLS_MAC_SHA, GNUTLS_MAC_MD5, 0 };
const static int cert_type_priority[16] = { GNUTLS_CRT_X509, GNUTLS_CRT_OPENPGP, 0 };

static void
init_gnutls(struct module *module)
{
	int ret = gnutls_global_init();

	if (ret < 0)
		INTERNAL("GNUTLS init failed: %s", gnutls_strerror(ret));

	ret = gnutls_anon_allocate_client_credentials(&anon_cred);
	if (ret < 0)
		INTERNAL("GNUTLS anon credentials alloc failed: %s",
			 gnutls_strerror(ret));

	ret = gnutls_certificate_allocate_credentials(&xcred);
	if (ret < 0)
		INTERNAL("GNUTLS X509 credentials alloc failed: %s",
			 gnutls_strerror(ret));

	/* Here, we should load certificate files etc. */
}

static void
done_gnutls(struct module *module)
{
	if (xcred) gnutls_certificate_free_credentials(xcred);
	if (anon_cred) gnutls_anon_free_client_credentials(anon_cred);
	gnutls_global_deinit();
}

static struct option_info gnutls_options[] = {
	INIT_OPT_BOOL("connection.ssl", N_("Verify certificates"),
		"cert_verify", 0, 0,
		N_("Verify the peer's SSL certificate. Note that this\n"
		"probably doesn't work properly at all with GnuTLS.")),

	NULL_OPTION_INFO,
};

static struct module gnutls_module = struct_module(
	/* name: */		"GnuTLS",
	/* options: */		gnutls_options,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_gnutls,
	/* done: */		done_gnutls
);

#endif /* CONFIG_OPENSSL or CONFIG_GNUTLS */

static struct option_info ssl_options[] = {
	INIT_OPT_TREE("connection", N_("SSL"),
		"ssl", OPT_SORT,
		N_("SSL options.")),

	NULL_OPTION_INFO,
};

static struct module *ssl_modules[] = {
#ifdef CONFIG_OPENSSL
	&openssl_module,
#elif defined(CONFIG_GNUTLS)
	&gnutls_module,
#endif
	NULL,
};

struct module ssl_module = struct_module(
	/* name: */		N_("SSL"),
	/* options: */		ssl_options,
	/* events: */		NULL,
	/* submodules: */	ssl_modules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);

int
init_ssl_connection(struct socket *socket)
{
#ifdef CONFIG_OPENSSL
	socket->ssl = SSL_new(context);
	if (!socket->ssl) return S_SSL_ERROR;
#elif defined(CONFIG_GNUTLS)
	const unsigned char server_name[] = "localhost";
	ssl_t *state = mem_alloc(sizeof(ssl_t));

	if (!state) return S_SSL_ERROR;

	if (gnutls_init(state, GNUTLS_CLIENT) < 0) {
		/* DBG("sslinit %s", gnutls_strerror(ret)); */
		mem_free(state);
		return S_SSL_ERROR;
	}

	if (gnutls_cred_set(*state, GNUTLS_CRD_ANON, anon_cred) < 0) {
		/* DBG("sslanoncred %s", gnutls_strerror(ret)); */
		gnutls_deinit(*state);
		mem_free(state);
		return S_SSL_ERROR;
	}

	if (gnutls_cred_set(*state, GNUTLS_CRD_CERTIFICATE, xcred) < 0) {
		/* DBG("sslx509cred %s", gnutls_strerror(ret)); */
		gnutls_deinit(*state);
		mem_free(state);
		return S_SSL_ERROR;
	}

	gnutls_handshake_set_private_extensions(*state, 1);
	gnutls_cipher_set_priority(*state, cipher_priority);
	gnutls_compression_set_priority(*state, comp_priority);
	gnutls_kx_set_priority(*state, kx_priority);
	gnutls_protocol_set_priority(*state, protocol_priority);
	gnutls_mac_set_priority(*state, mac_priority);
	gnutls_certificate_type_set_priority(*state, cert_type_priority);
	gnutls_server_name_set(*state, GNUTLS_NAME_DNS, server_name,
			       sizeof(server_name) - 1);

	socket->ssl = state;
#endif

	return S_OK;
}

void
done_ssl_connection(struct socket *socket)
{
	ssl_t *ssl = socket->ssl;

	if (!ssl) return;
#ifdef CONFIG_OPENSSL
	SSL_free(ssl);
#elif defined(CONFIG_GNUTLS)
	gnutls_deinit(*ssl);
	mem_free(ssl);
#endif
	socket->ssl = NULL;
}

unsigned char *
get_ssl_connection_cipher(struct socket *socket)
{
	ssl_t *ssl = socket->ssl;
	struct string str;

	if (!init_string(&str)) return NULL;

#ifdef CONFIG_OPENSSL
	add_format_to_string(&str, "%ld-bit %s %s",
		SSL_get_cipher_bits(ssl, NULL),
		SSL_get_cipher_version(ssl),
		SSL_get_cipher_name(ssl));
#elif defined(CONFIG_GNUTLS)
	/* XXX: How to get other relevant parameters? */
	add_format_to_string(&str, "%s - %s - %s - %s - %s (compr: %s)",
		gnutls_protocol_get_name(gnutls_protocol_get_version(*ssl)),
		gnutls_kx_get_name(gnutls_kx_get(*ssl)),
		gnutls_cipher_get_name(gnutls_cipher_get(*ssl)),
		gnutls_mac_get_name(gnutls_mac_get(*ssl)),
		gnutls_certificate_type_get_name(gnutls_certificate_type_get(*ssl)),
		gnutls_compression_get_name(gnutls_compression_get(*ssl)));
#endif

	return str.source;
}
