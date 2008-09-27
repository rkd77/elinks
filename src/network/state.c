/* Status/error messages managment */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "network/connection.h"
#include "network/state.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


/* Global variables */
struct s_msg_dsc {
	int n;
	unsigned char *msg;
};
static const struct s_msg_dsc msg_dsc[] = {
	{S_WAIT,		N_("Waiting in queue")},
	{S_DNS,			N_("Looking up host")},
	{S_CONN,		N_("Making connection")},
	{S_SSL_NEG,		N_("SSL negotiation")},
	{S_SENT,		N_("Request sent")},
	{S_LOGIN,		N_("Logging in")},
	{S_GETH,		N_("Getting headers")},
	{S_PROC,		N_("Server is processing request")},
	{S_TRANS,		N_("Transferring")},
#ifdef CONFIG_BITTORRENT
	{S_RESUME,		N_("Resuming")},
	{S_CONN_PEERS,		N_("Connecting to peers")},
	{S_CONN_TRACKER,	N_("Connecting to tracker")},
#endif

	{S_WAIT_REDIR,		N_("Waiting for redirect confirmation")},
	{S_OK,			N_("OK")},
	{S_INTERRUPTED,		N_("Interrupted")},
	{S_EXCEPT,		N_("Socket exception")},
	{S_INTERNAL,		N_("Internal error")},
	{S_OUT_OF_MEM,		N_("Out of memory")},
	{S_NO_DNS,		N_("Host not found")},
	{S_CANT_WRITE,		N_("Error writing to socket")},
	{S_CANT_READ,		N_("Error reading from socket")},
	{S_MODIFIED,		N_("Data modified")},
	{S_BAD_URL,		N_("Bad URL syntax")},
	{S_TIMEOUT,		N_("Receive timeout")},
	{S_RESTART,		N_("Request must be restarted")},
	{S_STATE,		N_("Can't get socket state")},
	{S_LOCAL_ONLY,		N_("Only local connections are permitted")},
	{S_NO_FORCED_DNS,	N_("No host in the specified IP family was found")},
#if defined(CONFIG_GZIP) || defined(CONFIG_BZIP2)
	{S_ENCODE_ERROR,	N_("Error while decoding file. This might be caused\n"
				   "by the encoded file being corrupt.") },
#endif
	{S_UNKNOWN_PROTOCOL,	N_("This URL contains a protocol not yet known by ELinks.\n"
				   "You can configure an external handler for it through\n"
				   "the options system.")},

	{S_EXTERNAL_PROTOCOL,	N_("This URL contains a protocol that is not natively known\n"
				   "by ELinks which means that ELinks relies on external\n"
				   "programs for handling it. Downloading URLs using external\n"
				   "programs is not supported.")},

	{S_HTTP_ERROR,		N_("Bad HTTP response")},
	{S_HTTP_204,		N_("No content")},
	{S_HTTP_UPLOAD_RESIZED, N_("File was resized during upload")},

	{S_FILE_TYPE,		N_("Unknown file type")},
	{S_FILE_ERROR,		N_("Error opening file")},
	{S_FILE_CGI_BAD_PATH,	N_("CGI script not in CGI path")},
	{S_FILE_ANONYMOUS,	N_("Local file access is not allowed in anonymous mode")},

#ifdef CONFIG_FTP
	{S_FTP_ERROR,		N_("Bad FTP response")},
	{S_FTP_UNAVAIL,		N_("FTP service unavailable")},
	{S_FTP_LOGIN,		N_("Bad FTP login")},
	{S_FTP_PORT,		N_("FTP PORT command failed")},
	{S_FTP_NO_FILE,		N_("File not found")},
	{S_FTP_FILE_ERROR,	N_("FTP file error")},
#endif

#ifdef CONFIG_SSL
	{S_SSL_ERROR,		N_("SSL error")},
#else
	{S_SSL_ERROR,		N_("This version of ELinks does not contain SSL/TLS support")},
#endif

	{S_NO_JAVASCRIPT,	N_("JavaScript support is not enabled")},

#ifdef CONFIG_NNTP
	{S_NNTP_ERROR,		N_("Bad NNTP response")},
	{S_NNTP_NEWS_SERVER,	N_("Unable to handle news: URI because no news server has been\n"
				"been configured. Either set the option protocol.nntp.server\n"
				"or set the NNTPSERVER environment variable.")},
	{S_NNTP_SERVER_HANG_UP,	N_("Server hung up for some reason")},
	{S_NNTP_GROUP_UNKNOWN,	N_("No such newsgroup")},
	{S_NNTP_ARTICLE_UNKNOWN,N_("No such article")},
	{S_NNTP_TRANSFER_ERROR,	N_("Transfer failed")},
	{S_NNTP_AUTH_REQUIRED,	N_("Authorization required")},
	{S_NNTP_ACCESS_DENIED,	N_("Access to server denied")},
#endif

#ifdef CONFIG_GOPHER
	{S_GOPHER_CSO_ERROR,	N_("The CSO phone-book protocol is not supported.")},
#endif

	{S_PROXY_ERROR,		N_("Configuration of the proxy server failed.\n"
				"This might be caused by an incorrect proxy\n"
				"setting specified by an environment variable\n"
				"or returned by a scripting proxy hook.\n"
				"\n"
				"The correct syntax for proxy settings are\n"
				"a host name optionally followed by a colon\n"
				"and a port number. Example: 'localhost:8080'.")},

#ifdef CONFIG_BITTORRENT
	{S_BITTORRENT_ERROR,	N_("BitTorrent error")},
	{S_BITTORRENT_METAINFO,	N_("The BitTorrent metainfo file contained errors")},
	{S_BITTORRENT_TRACKER,	N_("The tracker requesting failed")},
	{S_BITTORRENT_BAD_URL,	N_("The BitTorrent URL does not point to a valid URL")},
	{S_BITTORRENT_PEER_URL, N_("The bittorrent-peer URL scheme is for internal use only")},
#endif

	/* fsp_open_session() failed but did not set errno.
	 * fsp_open_session() never sends anything to the FSP server,
	 * so this error does not mean the server itself does not work.
	 * More likely, there was some problem in asking a DNS server
	 * about the address of the FSP server.  */
	{S_FSP_OPEN_SESSION_UNKN, N_("FSP server not found")},

	{0,			NULL}
};


struct strerror_val {
	LIST_HEAD(struct strerror_val);

	unsigned char msg[1]; /* must be last */
};

static INIT_LIST_OF(struct strerror_val, strerror_buf);

/* It returns convenient error message, depending on @state.
 * It never returns NULL (if one changes that, be warn that
 * callers may not test for this condition) --Zas */
unsigned char *
get_state_message(struct connection_state state, struct terminal *term)
{
	unsigned char *e;
	struct strerror_val *s;
	int len;
	unsigned char *unknown_error = _("Unknown error", term);

	if (!is_system_error(state)) {
		int i;

		for (i = 0; msg_dsc[i].msg; i++)
			if (msg_dsc[i].n == state.basic)
				return _(msg_dsc[i].msg, term);

		return unknown_error;
	}

	e = (unsigned char *) strerror(state.syserr);
	if (!e || !*e) return unknown_error;

	len = strlen(e);

	foreach (s, strerror_buf)
		if (!strlcmp(s->msg, -1, e, len))
			return s->msg;

	s = mem_calloc(1, sizeof(*s) + len);
	if (!s) return unknown_error;

	memcpy(s->msg, e, len + 1);
	add_to_list(strerror_buf, s);

	return s->msg;
}

void
done_state_message(void)
{
	free_list(strerror_buf);
}
