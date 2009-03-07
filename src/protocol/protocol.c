/* Protocol implementation manager. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "network/connection.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/memory.h"
#include "util/string.h"

/* Backends dynamic area: */

#include "protocol/about.h"
#include "protocol/auth/auth.h"
#include "protocol/bittorrent/bittorrent.h"
#include "protocol/bittorrent/connection.h"
#include "protocol/data.h"
#include "protocol/file/cgi.h"
#include "protocol/file/file.h"
#include "protocol/finger/finger.h"
#include "protocol/fsp/fsp.h"
#include "protocol/ftp/ftp.h"
#include "protocol/gopher/gopher.h"
#include "protocol/http/http.h"
#include "protocol/nntp/connection.h"
#include "protocol/nntp/nntp.h"
#include "protocol/rewrite/rewrite.h"
#include "protocol/smb/smb.h"
#include "protocol/user.h"


struct protocol_backend {
	unsigned char *name;
	int port;
	protocol_handler_T *handler;
	unsigned int need_slashes:1;
	unsigned int need_slash_after_host:1;
	unsigned int free_syntax:1;
	unsigned int need_ssl:1;
	unsigned int keep_double_slashes:1;
};

static const struct protocol_backend protocol_backends[] = {
	{ "about",	   0, about_protocol_handler,		0, 0, 1, 0, 1 },
	{ "bittorrent",	   0, bittorrent_protocol_handler,	0, 0, 1, 0, 1 },
	{ "bittorrent-peer",0,bittorrent_peer_protocol_handler, 1, 1, 0, 0, 1 },
	{ "data",	   0, data_protocol_handler,		0, 0, 1, 0, 1 },
	{ "file",	   0, file_protocol_handler,		1, 0, 0, 0, 0 },
	{ "finger",	  79, finger_protocol_handler,		1, 1, 0, 0, 1 },
	{ "fsp",	  21, fsp_protocol_handler,		1, 1, 0, 0, 1 },
	{ "ftp",	  21, ftp_protocol_handler,		1, 1, 0, 0, 0 },
	{ "gopher",	  70, gopher_protocol_handler,		1, 1, 0, 0, 1 },
	{ "http",	  80, http_protocol_handler,		1, 1, 0, 0, 1 },
	{ "https",	 443, https_protocol_handler,		1, 1, 0, 1, 1 },
	{ "javascript",	   0, NULL,				0, 0, 1, 0, 1 },
	{ "news",	   0, news_protocol_handler,		0, 0, 1, 0, 1 },
	{ "nntp",	 119, nntp_protocol_handler,		1, 1, 0, 0, 0 },
	{ "nntps",	 563, nntp_protocol_handler,		1, 1, 0, 1, 0 },
	{ "proxy",	3128, proxy_protocol_handler,		1, 1, 0, 0, 1 },
	{ "smb",	 139, smb_protocol_handler,		1, 1, 0, 0, 1 },
	{ "snews",	   0, news_protocol_handler,		0, 0, 1, 0, 1 },

	/* Keep these last! */
	{ NULL,		   0, NULL,			0, 0, 1, 0, 1 },

	{ "user",	   0, NULL,			0, 0, 0, 0, 1 },
	/* Internal protocol for mapping to protocol.user.* handlers. Placed
	 * last because it's checked first and else should be ignored. */
	{ "custom",	   0, NULL,			0, 0, 1, 0, 1 },
};


/* This function gets called quite a lot these days. With incremental rendering
 * and all I counted 4400 calls alone when loading fm. With the old linear
 * comparison this would lead to 30800 comparison against protocol names. The
 * binary search used currently reduces it to 4400 (meaning fm only has HTTP
 * links). */

enum protocol
get_protocol(unsigned char *name, int namelen)
{
	/* These are really enum protocol values but can take on negative
	 * values and since 0 <= -1 for enum values it's better to use clean
	 * integer type. */
	int start, end;
	enum protocol protocol;

	/* Almost dichotomic search is used here */
	/* Starting at the HTTP entry which is the most common that will make
	 * file and NNTP the next entries checked and amongst the third checks
	 * are proxy and FTP. */
	start	 = 0;
	end	 = PROTOCOL_UNKNOWN - 1;
	protocol = PROTOCOL_HTTP;

	assert(start <= protocol && protocol <= end);

	while (start <= end) {
		unsigned char *pname = protocol_backends[protocol].name;
		int pnamelen = strlen(pname);
		int minlen = int_min(pnamelen, namelen);
		int compare = c_strncasecmp(pname, name, minlen);

		if (compare == 0) {
			if (pnamelen == namelen)
				return protocol;

			/* If the current protocol name is longer than the
			 * protocol name being searched for move @end else move
			 * @start. */
			compare = pnamelen > namelen ? 1 : -1;
		}

		if (compare > 0)
			end = protocol - 1;
		else
			start = protocol + 1;

		protocol = (start + end) / 2;
	}
	/* Custom (protocol.user) protocol has higher precedence than builtin
	 * handlers, but we will check for it when following a link.
	 * Calling get_user_program for every link is too expensive. --witekfl */
	/* TODO: In order to fully give higher precedence to user chosen
	 *	 protocols we have to get some terminal to pass along. */

	if (get_user_program(NULL, name, namelen))
		return PROTOCOL_USER;

	return PROTOCOL_UNKNOWN;
}


#define VALID_PROTOCOL(p) (0 <= (p) && (p) < PROTOCOL_BACKENDS)

int
get_protocol_port(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;

	assert(uri_port_is_valid(protocol_backends[protocol].port));
	if_assert_failed return 0;

	return protocol_backends[protocol].port;
}

int
get_protocol_need_slashes(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol].need_slashes;
}

int
get_protocol_need_slash_after_host(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol].need_slash_after_host;
}

int
get_protocol_keep_double_slashes(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol].keep_double_slashes;
}

int
get_protocol_free_syntax(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol].free_syntax;
}

int
get_protocol_need_ssl(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return 0;
	return protocol_backends[protocol].need_ssl;
}

protocol_handler_T *
get_protocol_handler(enum protocol protocol)
{
	assert(VALID_PROTOCOL(protocol));
	if_assert_failed return NULL;
	return protocol_backends[protocol].handler;
}


static void
generic_external_protocol_handler(struct session *ses, struct uri *uri)
{
	/* [gettext_accelerator_context(generic_external_protocol_handler)] */
	struct connection_state state;

	switch (uri->protocol) {
	case PROTOCOL_JAVASCRIPT:
#ifdef CONFIG_ECMASCRIPT
		ecmascript_protocol_handler(ses, uri);
		return;
#else
		state = connection_state(S_NO_JAVASCRIPT);
#endif
		break;

	case PROTOCOL_UNKNOWN:
		state = connection_state(S_UNKNOWN_PROTOCOL);
		break;

	default:
#ifndef CONFIG_SSL
		if (get_protocol_need_ssl(uri->protocol)) {
			state = connection_state(S_SSL_ERROR);
			break;
		}
#endif
		msg_box(ses->tab->term, NULL, MSGBOX_FREE_TEXT,
			N_("Error"), ALIGN_CENTER,
			msg_text(ses->tab->term,
				N_("This version of ELinks does not contain "
				   "%s protocol support"),
				protocol_backends[uri->protocol].name),
			ses, 1,
			MSG_BOX_BUTTON(N_("~OK"), NULL, B_ENTER | B_ESC));
		return;
	}

	print_error_dialog(ses, state, uri, PRI_CANCEL);
}

protocol_external_handler_T *
get_protocol_external_handler(struct terminal *term, struct uri *uri)
{
	unsigned char *prog;

	assert(uri && VALID_PROTOCOL(uri->protocol));
	if_assert_failed return NULL;

	prog = get_user_program(term, struri(uri), uri->protocollen);
	if (prog && *prog)
		return user_protocol_handler;

	if (!protocol_backends[uri->protocol].handler)
		return generic_external_protocol_handler;

	return NULL;
}


static struct option_info protocol_options[] = {
	INIT_OPT_TREE("", N_("Protocols"),
		"protocol", OPT_SORT,
		N_("Protocol specific options.")),

	INIT_OPT_STRING("protocol", N_("No-proxy domains"),
		"no_proxy", 0, "",
		N_("Comma separated list of domains for which the proxy "
		"(HTTP/FTP) should be disabled. Optionally, a port can be "
		"specified for some domains as well. If it's blank, "
		"NO_PROXY environment variable is checked as well.")),

	NULL_OPTION_INFO,
};
static struct module *protocol_submodules[] = {
	&auth_module,
#ifdef CONFIG_BITTORRENT
	&bittorrent_protocol_module,
#endif
	&file_protocol_module,
#ifdef CONFIG_CGI
	&cgi_protocol_module,
#endif
#ifdef CONFIG_FINGER
	&finger_protocol_module,
#endif
#ifdef CONFIG_FSP
	&fsp_protocol_module,
#endif
#ifdef CONFIG_FTP
	&ftp_protocol_module,
#endif
#ifdef CONFIG_GOPHER
	&gopher_protocol_module,
#endif
	&http_protocol_module,
#ifdef CONFIG_NNTP
	&nntp_protocol_module,
#endif
#ifdef CONFIG_SMB
	&smb_protocol_module,
#endif
#ifdef CONFIG_URI_REWRITE
	&uri_rewrite_module,
#endif
	&user_protocol_module,
	NULL,
};

struct module protocol_module = struct_module(
	/* name: */		N_("Protocol"),
	/* options: */		protocol_options,
	/* hooks: */		NULL,
	/* submodules: */	protocol_submodules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
