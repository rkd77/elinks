/*
 * HTTP Negotiate authentication method -- based on GSSAPI
 *
 * The Microsoft version with SPNEGO is unsupported. If you look for way how
 * extend this code with SPNEGO see libcurl or firefox source code where is
 * supported GSSAPI+SPNEGO.
 *
 * Copyright (C) 2006 Red Hat, Inc.
 * Karel Zak <kzak@redhat.com>
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>

#include <gssapi/gssapi.h>

#include "elinks.h"
#include "network/connection.h"
#include "protocol/uri.h"
#include "protocol/http/http.h"
#include "protocol/http/http_negotiate.h"
#include "util/base64.h"
#include "main/object.h"
#include "util/lists.h"

struct negotiate {
	OBJECT_HEAD(struct negotiate);

	struct uri 	*uri;

	int 		type; 			/* GSS-Negotiate or Negotiate or zero */
	OM_uint32	status;
	gss_ctx_id_t	context;
	gss_name_t	server_name;
	gss_buffer_desc output_token;
	gss_buffer_desc input_token;
};

static INIT_LIST_OF(struct negotiate, negotiate_list);

static struct negotiate *
http_negotiate_get(struct uri *uri, int *isnew, int alloc)
{
	struct negotiate *neg;

	foreach (neg, negotiate_list) {
		if (compare_uri(neg->uri, uri, URI_HTTP_REFERRER_HOST))
			return neg;
	}
	if (!alloc)
		return NULL;

	neg = mem_calloc(1, sizeof(*neg));
	if (!neg)
		return NULL;

	neg->uri = get_uri_reference(uri);

	if (isnew)
		*isnew = 1;

	return neg;
}

static void
http_negotiate_save(struct negotiate *neg)
{
	add_to_list(negotiate_list, neg);
}

static void
http_negotiate_cleanup(struct negotiate *neg, int full)
{
	OM_uint32 minor_status;

	if (neg->context != GSS_C_NO_CONTEXT)
		gss_delete_sec_context(&minor_status, &neg->context, GSS_C_NO_BUFFER);

	if (neg->output_token.length != 0)
		gss_release_buffer(&minor_status, &neg->output_token);

	if (full) {
		if (neg->server_name)
			gss_release_name(&minor_status, &neg->server_name);

		if (neg->input_token.length != 0) {
			/* allocated by mem_free().. so beter not use gss_release_buffer() */
			mem_free(neg->input_token.value);
			neg->input_token.length = 0;
		}

		memset(neg, 0, sizeof(*neg));
	}
}

static int
http_negotiate_get_name(struct connection *conn, struct negotiate *neg)
{
	OM_uint32 major_status, minor_status;
	gss_buffer_desc token = GSS_C_EMPTY_BUFFER;
	char name[2048];
	const char *service;
	struct uri *uri = conn->proxied_uri;

	/* GSSAPI implementation by Globus (known as GSI) requires the name to be
	 * of form "<service>/<fqdn>" instead of <service>@<fqdn> (ie. slash instead
	 * of at-sign). Also GSI servers are often identified as 'host' not 'khttp'.
	 * Change following lines if you want to use GSI
	 *
	 * IIS uses the <service>@<fqdn> form but uses 'http' as the service name
	 */
	if (neg->type == HTTPNEG_GSS)
		service = "KHTTP";
	else
		service = "HTTP";

	token.length = strlen(service) + 1 + uri->hostlen + 1;
	if (token.length + 1 > sizeof(name))
		return -1;

	snprintf(name, token.length, "%s@%*s", service, uri->hostlen, uri->host);

	token.value = (void *) name;
	major_status = gss_import_name(&minor_status, &token,
				       GSS_C_NT_HOSTBASED_SERVICE,
				       &neg->server_name);

	return GSS_ERROR(major_status) ? -1 : 0;
}

static int
http_negotiate_parse_data(unsigned char *data, int type,
			  gss_buffer_desc *token)
{
	int len = 0;
	unsigned char *end;

	if (data == NULL || *data == '\0')
		return 0;

	if (type == HTTPNEG_GSS)
		data += HTTPNEG_GSS_STRLEN;
	else
		data += HTTPNEG_NEG_STRLEN;

	while (*data && isspace((int) *data))
		data++;

	if (*data == '\0' || *data == ASCII_CR || *data == ASCII_LF)
		return 0;	/* no data */

	end = data;
	while (isalnum((int) *end) || *end == '=')
		end++;

	/* Ignore line if we encountered an unexpected char. */
	if (*end != ASCII_CR && *end != ASCII_LF)
		return 0;

	len = end - data;

	if (!len)
		return 0;

	token->value = (void *) base64_decode_bin(data, len, &token->length);

	if (!token->value)
		return -1;

	return 0;
}

static int
http_negotiate_create_context(struct negotiate *neg)
{
	OM_uint32 major_status, minor_status;

	major_status = gss_init_sec_context(&minor_status,
					    GSS_C_NO_CREDENTIAL,
					    &neg->context,
					    neg->server_name,
					    GSS_C_NO_OID,
					    GSS_C_DELEG_FLAG,
					    0,
					    GSS_C_NO_CHANNEL_BINDINGS,
					    &neg->input_token,
					    NULL,
					    &neg->output_token,
					    NULL,
					    NULL);
	neg->status = major_status;

	if (GSS_ERROR(major_status) || neg->output_token.length == 0)
		return -1;

	return 0;
}

/*
 * Register new negotiate-auth request
 *
 * It's possible that server sends to client input token (at least
 * libcurl supports it) in WWW-Authenticate header, but ususaly
 * is this input token undefined.
 */
int
http_negotiate_input(struct connection *conn, struct uri *uri,
		     int type, unsigned char *data)
{
	struct negotiate *neg;
	int ret = 0, isnew = 0;

	neg = http_negotiate_get(uri, &isnew, 1);

	if (neg->context && type != HTTPNEG_GSS)
		return -1;

	neg->type = type;

	if (neg->context && neg->status == GSS_S_COMPLETE) {
		/* We finished succesfully our part of authentication, but
		 * server rejected it (since we're again here). Exit with an
		 * error since we can't invent anything better
		 */
		http_negotiate_cleanup(neg, 1);
		return -1;
	}

	if (neg->server_name == NULL && http_negotiate_get_name(conn, neg) < 0)
		return -1;

	if (data && http_negotiate_parse_data(data, type, &neg->input_token))
		return -1;

	ret = http_negotiate_create_context(neg);
	if (ret == 0 && isnew)
		http_negotiate_save(neg);

	return ret;
}

/*
 * Fill output token to "Authorization: Negotiate <token>".
 */
int
http_negotiate_output(struct uri *uri, struct string *header)
{
	struct negotiate *neg;
	char *encoded = NULL;
	int len = 0;

	neg = http_negotiate_get(uri, NULL, 0);
	if (!neg)
		return -1;

	if (neg->output_token.length == 0) {
		if (http_negotiate_create_context(neg) < 0) {
			/* full cleanup on error and ask for
			 * new WWW-Authenticate from server
			 */
			http_negotiate_cleanup(neg, 1);
			return -1;
		}
	}

	encoded = base64_encode_bin((unsigned char *) neg->output_token.value,
				    neg->output_token.length, &len);

	if (encoded == NULL || len == 0)
		return -1;

	add_to_string(header, "Authorization: ");
	add_to_string(header, neg->type == HTTPNEG_GSS ?
		      HTTPNEG_GSS_STR : HTTPNEG_NEG_STR);
	add_char_to_string(header, ' ');
	add_to_string(header, encoded);
	add_crlf_to_string(header);

	http_negotiate_cleanup(neg, 0);

	mem_free(encoded);

	return 0;
}
