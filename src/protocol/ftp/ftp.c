/* Internal "ftp" protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>	/* For converting permissions to strings */
#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif

/* We need to have it here. Stupid BSD. */
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/select.h"
#include "main/module.h"
#include "network/connection.h"
#include "network/progress.h"
#include "network/socket.h"
#include "osdep/osdep.h"
#include "osdep/stat.h"
#include "protocol/auth/auth.h"
#include "protocol/common.h"
#include "protocol/ftp/ftp.h"
#include "protocol/ftp/parse.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


struct option_info ftp_options[] = {
	INIT_OPT_TREE("protocol", N_("FTP"),
		"ftp", 0,
		N_("FTP specific options.")),

	INIT_OPT_TREE("protocol.ftp", N_("Proxy configuration"),
		"proxy", 0,
		N_("FTP proxy configuration.")),

	INIT_OPT_STRING("protocol.ftp.proxy", N_("Host and port-number"),
		"host", 0, "",
		N_("Host and port-number (host:port) of the FTP proxy, "
		"or blank. If it's blank, FTP_PROXY environment variable "
		"is checked as well.")),

	INIT_OPT_STRING("protocol.ftp", N_("Anonymous password"),
		"anon_passwd", 0, "some@host.domain",
		N_("FTP anonymous password to be sent.")),

	INIT_OPT_BOOL("protocol.ftp", N_("Use passive mode (IPv4)"),
		"use_pasv", 0, 1,
		N_("Use PASV instead of PORT (passive vs active mode, "
		"IPv4 only).")),
#ifdef CONFIG_IPV6
	INIT_OPT_BOOL("protocol.ftp", N_("Use passive mode (IPv6)"),
		"use_epsv", 0, 0,
		N_("Use EPSV instead of EPRT (passive vs active mode, "
		"IPv6 only).")),
#endif /* CONFIG_IPV6 */
	NULL_OPTION_INFO,
};


struct module ftp_protocol_module = struct_module(
	/* name: */		N_("FTP"),
	/* options: */		ftp_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);


/* Constants */

#define FTP_BUF_SIZE	16384


/* Types and structs */

struct ftp_connection_info {
	int pending_commands;        /* Num of commands queued */
	int opc;                     /* Total num of commands queued */
	int conn_state;
	int buf_pos;

	unsigned int dir:1;          /* Directory listing in progress */
	unsigned int rest_sent:1;    /* Sent RESTore command */
	unsigned int use_pasv:1;     /* Use PASV (yes or no) */
#ifdef CONFIG_IPV6
	unsigned int use_epsv:1;     /* Use EPSV */
#endif
	unsigned char ftp_buffer[FTP_BUF_SIZE];
	unsigned char cmd_buffer[1]; /* Must be last field !! */
};


/* Prototypes */
static void ftp_login(struct socket *);
static void ftp_send_retr_req(struct connection *, struct connection_state);
static void ftp_got_info(struct socket *, struct read_buffer *);
static void ftp_got_user_info(struct socket *, struct read_buffer *);
static void ftp_pass(struct connection *);
static void ftp_pass_info(struct socket *, struct read_buffer *);
static void ftp_retr_file(struct socket *, struct read_buffer *);
static void ftp_got_final_response(struct socket *, struct read_buffer *);
static void got_something_from_data_connection(struct connection *);
static void ftp_end_request(struct connection *, struct connection_state);
static struct ftp_connection_info *add_file_cmd_to_str(struct connection *);
static void ftp_data_accept(struct connection *conn);

/* Parse EPSV or PASV response for address and/or port.
 * int *n should point to a sizeof(int) * 6 space.
 * It returns zero on error or count of parsed numbers.
 * It returns an error if:
 * - there's more than 6 or less than 1 numbers.
 * - a number is strictly greater than max.
 *
 * On success, array of integers *n is filled with numbers starting
 * from end of array (ie. if we found one number, you can access it using
 * n[5]).
 *
 * Important:
 * Negative numbers aren't handled so -123 is taken as 123.
 * We don't take care about separators.
*/
static int
parse_psv_resp(unsigned char *data, int *n, int max_value)
{
	unsigned char *p = data;
	int i = 5;

	memset(n, 0, 6 * sizeof(*n));

	if (*p < ' ') return 0;

	/* Find the end. */
	while (*p >= ' ') p++;

	/* Ignore non-numeric ending chars. */
       	while (p != data && !isdigit(*p)) p--;
	if (p == data) return 0;

	while (i >= 0) {
		int x = 1;

		/* Parse one number. */
		while (p != data && isdigit(*p)) {
			n[i] += (*p - '0') * x;
			if (n[i] > max_value) return 0;
			x *= 10;
			p--;
		}
		/* Ignore non-numeric chars. */
		while (p != data && !isdigit(*p)) p--;
		if (p == data) return (6 - i);
		/* Get the next one. */
		i--;
	}

	return 0;
}

/* Returns 0 if there's no numeric response, -1 if error, the positive response
 * number otherwise. */
static int
get_ftp_response(struct connection *conn, struct read_buffer *rb, int part,
		 struct sockaddr_storage *sa)
{
	unsigned char *eol;
	unsigned char *num_end;
	int response;
	int pos;

again:
	eol = memchr(rb->data, ASCII_LF, rb->length);
	if (!eol) return 0;

	pos = eol - rb->data;

	errno = 0;
	response = strtoul(rb->data, (char **) &num_end, 10);

	if (errno || num_end != rb->data + 3 || response < 100)
		return -1;

	if (sa && response == 227) { /* PASV response parsing. */
		struct sockaddr_in *s = (struct sockaddr_in *) sa;
		int n[6];

		if (parse_psv_resp(num_end, (int *) &n, 255) != 6)
			return -1;

		memset(s, 0, sizeof(*s));
		s->sin_family = AF_INET;
		s->sin_addr.s_addr = htonl((n[0] << 24) + (n[1] << 16) + (n[2] << 8) + n[3]);
		s->sin_port = htons((n[4] << 8) + n[5]);
	}

#ifdef CONFIG_IPV6
	if (sa && response == 229) { /* EPSV response parsing. */
		/* See RFC 2428 */
		struct sockaddr_in6 *s = (struct sockaddr_in6 *) sa;
		int sal = sizeof(*s);
		int n[6];

		if (parse_psv_resp(num_end, (int *) &n, 65535) != 1) {
			return -1;
		}

		memset(s, 0, sizeof(*s));
		if (getpeername(conn->socket->fd, (struct sockaddr *) sa, &sal)) {
			return -1;
		}
		s->sin6_family = AF_INET6;
		s->sin6_port = htons(n[5]);
	}
#endif

	if (*num_end == '-') {
		int i;

		for (i = 0; i < rb->length - 5; i++)
			if (rb->data[i] == ASCII_LF
			    && !memcmp(rb->data+i+1, rb->data, 3)
			    && rb->data[i+4] == ' ') {
				for (i++; i < rb->length; i++)
					if (rb->data[i] == ASCII_LF)
						goto ok;
				return 0;
			}
		return 0;
ok:
		pos = i;
	}

	if (part != 2)
		kill_buffer_data(rb, pos + 1);

	if (!part && response >= 100 && response < 200) {
		goto again;
	}

	return response;
}


/* Initialize or continue ftp connection. */
void
ftp_protocol_handler(struct connection *conn)
{
	if (!has_keepalive_connection(conn)) {
		make_connection(conn->socket, conn->uri, ftp_login,
				conn->cache_mode >= CACHE_MODE_FORCE_RELOAD);

	} else {
		ftp_send_retr_req(conn, connection_state(S_SENT));
	}
}

/* Send command, set connection state and free cmd string. */
static void
send_cmd(struct connection *conn, struct string *cmd, void *callback,
	 struct connection_state state)
{
	request_from_socket(conn->socket, cmd->source, cmd->length, state,
			    SOCKET_RETRY_ONCLOSE, callback);

	done_string(cmd);
}

/* Check if this auth token really belongs to this URI. */
static int
auth_user_matching_uri(struct auth_entry *auth, struct uri *uri)
{
	if (!uri->userlen) /* Noone said it doesn't. */
		return 1;
	return !c_strlcasecmp(auth->user, -1, uri->user, uri->userlen);
}


/* Kill the current connection and ask for a username/password for the next
 * try. */
static void
prompt_username_pw(struct connection *conn)
{
	if (!conn->cached) {
		conn->cached = get_cache_entry(conn->uri);
		if (!conn->cached) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
	}

	mem_free_set(&conn->cached->content_type, stracpy("text/html"));
	if (!conn->cached->content_type) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	add_auth_entry(conn->uri, "FTP Login", NULL, NULL, 0);

	abort_connection(conn, connection_state(S_OK));
}

/* Send USER command. */
static void
ftp_login(struct socket *socket)
{
	struct connection *conn = socket->conn;
	struct string cmd;
	struct auth_entry* auth;

	auth = find_auth(conn->uri);

	if (!init_string(&cmd)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	add_to_string(&cmd, "USER ");
	if (conn->uri->userlen) {
		struct uri *uri = conn->uri;

		add_bytes_to_string(&cmd, uri->user, uri->userlen);

	} else if (auth && auth->valid) {
		add_to_string(&cmd, auth->user);

	} else {
		add_to_string(&cmd, "anonymous");
	}
	add_crlf_to_string(&cmd);

	send_cmd(conn, &cmd, (void *) ftp_got_info, connection_state(S_SENT));
}

/* Parse connection response. */
static void
ftp_got_info(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	int response = get_ftp_response(conn, rb, 0, NULL);

	if (response == -1) {
		abort_connection(conn, connection_state(S_FTP_ERROR));
		return;
	}

	if (!response) {
		read_from_socket(conn->socket, rb, conn->state, ftp_got_info);
		return;
	}

	/* RFC959 says that possible response codes on connection are:
	 * 120 Service ready in nnn minutes.
	 * 220 Service ready for new user.
	 * 421 Service not available, closing control connection. */

	if (response != 220) {
		/* TODO? Retry in case of ... ?? */
		retry_connection(conn, connection_state(S_FTP_UNAVAIL));
		return;
	}

	ftp_got_user_info(socket, rb);
}


/* Parse USER response and send PASS command if needed. */
static void
ftp_got_user_info(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	int response = get_ftp_response(conn, rb, 0, NULL);

	if (response == -1) {
		abort_connection(conn, connection_state(S_FTP_ERROR));
		return;
	}

	if (!response) {
		read_from_socket(conn->socket, rb, conn->state, ftp_got_user_info);
		return;
	}

	/* RFC959 says that possible response codes for USER are:
	 * 230 User logged in, proceed.
	 * 331 User name okay, need password.
	 * 332 Need account for login.
	 * 421 Service not available, closing control connection.
	 * 500 Syntax error, command unrecognized.
	 * 501 Syntax error in parameters or arguments.
	 * 530 Not logged in. */

	/* FIXME? Since ACCT command isn't implemented, login to a ftp server
	 * requiring it will fail (332). */

	if (response == 332 || response >= 500) {
		prompt_username_pw(conn);
		return;
	}

	/* We don't require exact match here, as this is always error and some
	 * non-RFC compliant servers may return even something other than 421.
	 * --Zas */
	if (response >= 400) {
		abort_connection(conn, connection_state(S_FTP_UNAVAIL));
		return;
	}

	if (response == 230) {
		ftp_send_retr_req(conn, connection_state(S_GETH));
		return;
	}

	ftp_pass(conn);
}

/* Send PASS command. */
static void
ftp_pass(struct connection *conn)
{
	struct string cmd;
	struct auth_entry *auth;

	auth = find_auth(conn->uri);

	if (!init_string(&cmd)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	add_to_string(&cmd, "PASS ");
	if (conn->uri->passwordlen) {
		struct uri *uri = conn->uri;

		add_bytes_to_string(&cmd, uri->password, uri->passwordlen);

	} else if (auth && auth->valid) {
		if (!auth_user_matching_uri(auth, conn->uri)) {
			prompt_username_pw(conn);
			return;
		}
		add_to_string(&cmd, auth->password);

	} else {
		add_to_string(&cmd, get_opt_str("protocol.ftp.anon_passwd"));
	}
	add_crlf_to_string(&cmd);

	send_cmd(conn, &cmd, (void *) ftp_pass_info, connection_state(S_LOGIN));
}

/* Parse PASS command response. */
static void
ftp_pass_info(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	int response = get_ftp_response(conn, rb, 0, NULL);

	if (response == -1) {
		abort_connection(conn, connection_state(S_FTP_ERROR));
		return;
	}

	if (!response) {
		read_from_socket(conn->socket, rb, connection_state(S_LOGIN),
				 ftp_pass_info);
		return;
	}

	/* RFC959 says that possible response codes for PASS are:
	 * 202 Command not implemented, superfluous at this site.
	 * 230 User logged in, proceed.
	 * 332 Need account for login.
	 * 421 Service not available, closing control connection.
	 * 500 Syntax error, command unrecognized.
	 * 501 Syntax error in parameters or arguments.
	 * 503 Bad sequence of commands.
	 * 530 Not logged in. */

	if (response == 332 || response >= 500) {
		/* If we didn't have a user, we tried anonymous. But it failed, so ask for a
		 * user and password */
		prompt_username_pw(conn);
		return;
	}

	if (response >= 400) {
		abort_connection(conn, connection_state(S_FTP_UNAVAIL));
		return;
	}

	ftp_send_retr_req(conn, connection_state(S_GETH));
}

/* Construct PORT command. */
static void
add_portcmd_to_string(struct string *string, unsigned char *pc)
{
	/* From RFC 959: DATA PORT (PORT)
	 *
	 * The argument is a HOST-PORT specification for the data port
	 * to be used in data connection.  There are defaults for both
	 * the user and server data ports, and under normal
	 * circumstances this command and its reply are not needed.  If
	 * this command is used, the argument is the concatenation of a
	 * 32-bit internet host address and a 16-bit TCP port address.
	 * This address information is broken into 8-bit fields and the
	 * value of each field is transmitted as a decimal number (in
	 * character string representation).  The fields are separated
	 * by commas.  A port command would be:
	 *
	 *    PORT h1,h2,h3,h4,p1,p2
	 *
	 * where h1 is the high order 8 bits of the internet host
	 * address. */
	add_to_string(string, "PORT ");
	add_long_to_string(string, pc[0]);
	add_char_to_string(string, ',');
	add_long_to_string(string, pc[1]);
	add_char_to_string(string, ',');
	add_long_to_string(string, pc[2]);
	add_char_to_string(string, ',');
	add_long_to_string(string, pc[3]);
	add_char_to_string(string, ',');
	add_long_to_string(string, pc[4]);
	add_char_to_string(string, ',');
	add_long_to_string(string, pc[5]);
}

#ifdef CONFIG_IPV6
/* Construct EPRT command. */
static void
add_eprtcmd_to_string(struct string *string, struct sockaddr_in6 *addr)
{
	unsigned char addr_str[INET6_ADDRSTRLEN];

	inet_ntop(AF_INET6, &addr->sin6_addr, addr_str, INET6_ADDRSTRLEN);

	/* From RFC 2428: EPRT
	 *
	 * The format of EPRT is:
	 *
	 * EPRT<space><d><net-prt><d><net-addr><d><tcp-port><d>
	 *
	 * <net-prt>:
	 * AF Number   Protocol
	 * ---------   --------
	 * 1           Internet Protocol, Version 4 [Pos81a]
	 * 2           Internet Protocol, Version 6 [DH96] */
	add_to_string(string, "EPRT |2|");
	add_to_string(string, addr_str);
	add_char_to_string(string, '|');
	add_long_to_string(string, ntohs(addr->sin6_port));
	add_char_to_string(string, '|');
}
#endif

/* Depending on options, get proper ftp data socket and command.
 * It appends ftp command (either PASV,PORT,EPSV or EPRT) to @command
 * string.
 * When PORT or EPRT are used, related sockets are created.
 * It returns 0 on error (data socket creation failure). */
static int
get_ftp_data_socket(struct connection *conn, struct string *command)
{
	struct ftp_connection_info *ftp = conn->info;

	ftp->use_pasv = get_opt_bool("protocol.ftp.use_pasv");

#ifdef CONFIG_IPV6
	ftp->use_epsv = get_opt_bool("protocol.ftp.use_epsv");

	if (conn->socket->protocol_family == EL_PF_INET6) {
		if (ftp->use_epsv) {
			add_to_string(command, "EPSV");

		} else {
			struct sockaddr_storage data_addr;
			int data_sock;

			memset(&data_addr, 0, sizeof(data_addr));
			data_sock = get_pasv_socket(conn->socket, &data_addr);
			if (data_sock < 0) return 0;

			conn->data_socket->fd = data_sock;
			add_eprtcmd_to_string(command,
					      (struct sockaddr_in6 *) &data_addr);
		}

	} else
#endif
	{
		if (ftp->use_pasv) {
			add_to_string(command, "PASV");

		} else {
			struct sockaddr_in sa;
			unsigned char pc[6];
			int data_sock;

			memset(pc, 0, sizeof(pc));
			data_sock = get_pasv_socket(conn->socket,
			 	    (struct sockaddr_storage *) &sa);
			if (data_sock < 0) return 0;

			memcpy(pc, &sa.sin_addr.s_addr, 4);
			memcpy(pc + 4, &sa.sin_port, 2);
			conn->data_socket->fd = data_sock;
			add_portcmd_to_string(command, pc);
		}
	}

	add_crlf_to_string(command);

	return 1;
}

/* Check if the file or directory name @s can be safely sent to the
 * FTP server.  To prevent command injection attacks, this function
 * must reject CR LF sequences.  */
static int
is_ftp_pathname_safe(const struct string *s)
{
	int i;

	/* RFC 959 says the argument of CWD and RETR is a <pathname>,
	 * which consists of <char>s, "any of the 128 ASCII characters
	 * except <CR> and <LF>".  So other control characters, such
	 * as 0x00 and 0x7F, are allowed here.  Bytes 0x80...0xFF
	 * should not be allowed, but if we reject them, users will
	 * probably complain.  */
	for (i = 0; i < s->length; i++) {
		if (s->source[i] == 0x0A || s->source[i] == 0x0D)
			return 0;
	}
	return 1;
}

/* Create passive socket and add appropriate announcing commands to str. Then
 * go and retrieve appropriate object from server.
 * Returns NULL if error. */
static struct ftp_connection_info *
add_file_cmd_to_str(struct connection *conn)
{
	int ok = 0;
	struct ftp_connection_info *ftp = NULL;
	struct string command = NULL_STRING;
	struct string ftp_data_command = NULL_STRING;
	struct string pathname = NULL_STRING;

	if (!conn->uri->data) {
		INTERNAL("conn->uri->data empty");
		abort_connection(conn, connection_state(S_INTERNAL));
		goto ret;
	}

	/* This will be reallocated below when we know how long the
	 * command string should be.  Error handling could be
	 * simplified a little by allocating this initial structure on
	 * the stack, but it's several kilobytes long so that might be
	 * risky.  */
	ftp = mem_calloc(1, sizeof(*ftp));
	if (!ftp) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		goto ret;
	}

	conn->info = ftp;	/* Freed when connection is destroyed. */

	if (!init_string(&command)
	    || !init_string(&ftp_data_command)
	    || !init_string(&pathname)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		goto ret;
	}

	if (!get_ftp_data_socket(conn, &ftp_data_command)) {
		INTERNAL("Ftp data socket failure");
		abort_connection(conn, connection_state(S_INTERNAL));
		goto ret;
	}

	if (!add_uri_to_string(&pathname, conn->uri, URI_PATH)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		goto ret;
	}

	decode_uri_string(&pathname);
	if (!is_ftp_pathname_safe(&pathname)) {
		abort_connection(conn, connection_state(S_BAD_URL));
		goto ret;
	}

	if (!conn->uri->datalen
	    || conn->uri->data[conn->uri->datalen - 1] == '/') {
		/* Commands to get directory listing. */

		ftp->dir = 1;
		ftp->pending_commands = 4;

		if (!add_to_string(&command, "TYPE A") /* ASCII */
		    || !add_crlf_to_string(&command)

		    || !add_string_to_string(&command, &ftp_data_command)

		    || !add_to_string(&command, "CWD ")
		    || !add_string_to_string(&command, &pathname)
		    || !add_crlf_to_string(&command)

		    || !add_to_string(&command, "LIST")
		    || !add_crlf_to_string(&command)) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			goto ret;
		}

		conn->from = 0;

	} else {
		/* Commands to get a file. */

		ftp->dir = 0;
		ftp->pending_commands = 3;

		if (!add_to_string(&command, "TYPE I") /* BINARY */
		    || !add_crlf_to_string(&command)

		    || !add_string_to_string(&command, &ftp_data_command)) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			goto ret;
		}

		if (conn->from || conn->progress->start > 0) {
			const off_t offset = conn->from
				? conn->from
				: conn->progress->start;

			if (!add_to_string(&command, "REST ")
			    || !add_long_to_string(&command, offset)
			    || !add_crlf_to_string(&command)) {
				abort_connection(conn, connection_state(S_OUT_OF_MEM));
				goto ret;
			}

			ftp->rest_sent = 1;
			ftp->pending_commands++;
		}

		if (!add_to_string(&command, "RETR ")
		    || !add_string_to_string(&command, &pathname)
		    || !add_crlf_to_string(&command)) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			goto ret;
		}
	}

	ftp->opc = ftp->pending_commands;

	/* 1 byte is already reserved for cmd_buffer in struct ftp_connection_info. */
	ftp = mem_realloc(ftp, sizeof(*ftp) + command.length);
	if (!ftp) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		goto ret;
	}
	conn->info = ftp;   /* in case mem_realloc moved the buffer */

	memcpy(ftp->cmd_buffer, command.source, command.length + 1);
	ok = 1;

ret:
	/* If @ok is false here, then abort_connection has already
	 * freed @ftp, which now is a dangling pointer.  */
	done_string(&pathname);
	done_string(&ftp_data_command);
	done_string(&command);
	return ok ? ftp : NULL;
}

static void
send_it_line_by_line(struct connection *conn, struct string *cmd)
{
	struct ftp_connection_info *ftp = conn->info;
	unsigned char *nl = strchr(ftp->cmd_buffer, '\n');

	if (!nl) {
		add_to_string(cmd, ftp->cmd_buffer);
		return;
	}

	nl++;
	add_bytes_to_string(cmd, ftp->cmd_buffer, nl - ftp->cmd_buffer);
	memmove(ftp->cmd_buffer, nl, strlen(nl) + 1);
}

/* Send commands to retrieve file or directory. */
static void
ftp_send_retr_req(struct connection *conn, struct connection_state state)
{
	struct string cmd;

	if (!init_string(&cmd)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	/* We don't save return value from add_file_cmd_to_str(), as it's saved
	 * in conn->info as well. */
	if (!conn->info && !add_file_cmd_to_str(conn)) {
		done_string(&cmd);
		return;
	}

	/* Send it line-by-line. */
	send_it_line_by_line(conn, &cmd);

	send_cmd(conn, &cmd, (void *) ftp_retr_file, state);
}

/* Parse RETR response and return file size or -1 on error. */
static off_t
get_filesize_from_RETR(unsigned char *data, int data_len, int *resume)
{
	off_t file_len;
	int pos;
	int pos_file_len = 0;

	/* Getting file size from text response.. */
	/* 150 Opening BINARY mode data connection for hello-1.0-1.1.diff.gz (16452 bytes). */

	*resume = 0;
	for (pos = 0; pos < data_len && data[pos] != ASCII_LF; pos++)
		if (data[pos] == '(')
			pos_file_len = pos;

	if (!pos_file_len || pos_file_len == data_len - 1) {
	/* Getting file size from ftp.task.gda.pl */
	/* 150 5676.4 kbytes to download */
		unsigned char tmp = data[data_len - 1];
		unsigned char *kbytes;
		char *endptr;
		double size;

		data[data_len - 1] = '\0';
		kbytes = strstr(data, "kbytes");
		data[data_len - 1] = tmp;
		if (!kbytes) return -1;

		for (kbytes -= 2; kbytes >= data; kbytes--) {
			if (*kbytes == ' ') break;
		}
		if (*kbytes != ' ') return -1;
		kbytes++;
		size = strtod((const char *)kbytes, &endptr);
		if (endptr == (char *)kbytes) return -1;
		*resume = 1;
		return (off_t)(size * 1024.0);
	}

	pos_file_len++;
	if (!isdigit(data[pos_file_len]))
		return -1;

	for (pos = pos_file_len; pos < data_len; pos++)
		if (!isdigit(data[pos]))
			goto next;

	return -1;

next:
	for (; pos < data_len; pos++)
		if (data[pos] != ' ')
			break;

	if (pos + 4 > data_len)
		return -1;

	if (c_strncasecmp(&data[pos], "byte", 4))
		return -1;

	errno = 0;
	file_len = (off_t) strtol(&data[pos_file_len], NULL, 10);
	if (errno) return -1;

	return file_len;
}

/* Connect to the host and port specified by a passive FTP server.  */
static int
ftp_data_connect(struct connection *conn, int pf, struct sockaddr_storage *sa,
		 int size_of_sockaddr)
{
	int fd;

	if (conn->data_socket->fd != -1) {
		/* The server maliciously sent multiple 227 or 229
		 * responses.  Do not leak the previous data_socket.  */
		abort_connection(conn, connection_state(S_FTP_ERROR));
		return -1;
	}

	fd = socket(pf, SOCK_STREAM, 0);
	if (fd < 0 || set_nonblocking_fd(fd) < 0) {
		abort_connection(conn, connection_state(S_FTP_ERROR));
		return -1;
	}

	set_ip_tos_throughput(fd);

	conn->data_socket->fd = fd;
	/* XXX: We ignore connect() errors here. */
	connect(fd, (struct sockaddr *) sa, size_of_sockaddr);
	return 0;
}

static void
ftp_retr_file(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	struct ftp_connection_info *ftp = conn->info;
	int response;

	if (ftp->pending_commands > 1) {
		struct sockaddr_storage sa;

		response = get_ftp_response(conn, rb, 0, &sa);

		if (response == -1) {
			abort_connection(conn, connection_state(S_FTP_ERROR));
			return;
		}

		if (!response) {
			read_from_socket(conn->socket, rb,
					 connection_state(S_GETH),
					 ftp_retr_file);
			return;
		}

		if (response == 227) {
			if (ftp_data_connect(conn, PF_INET, &sa, sizeof(struct sockaddr_in)))
				return;
		}

#ifdef CONFIG_IPV6
		if (response == 229) {
			if (ftp_data_connect(conn, PF_INET6, &sa, sizeof(struct sockaddr_in6)))
				return;
		}
#endif

		ftp->pending_commands--;

		/* XXX: The case values are order numbers of commands. */
		switch (ftp->opc - ftp->pending_commands) {
			case 1:	/* TYPE */
				break;

			case 2:	/* PORT */
				if (response >= 400) {
					abort_connection(conn,
							 connection_state(S_FTP_PORT));
					return;
				}
				break;

			case 3:	/* REST / CWD */
				if (response >= 400) {
					if (ftp->dir) {
						abort_connection(conn,
								 connection_state(S_FTP_NO_FILE));
						return;
					}
					conn->from = 0;
				} else if (ftp->rest_sent) {
					/* Following code is related to resume
					 * feature. */
					if (response == 350)
						conn->from = conn->progress->start;
					/* Come on, don't be nervous ;-). */
					if (conn->progress->start >= 0) {
						/* Update to the real value
						 * which we've got from
						 * Content-Range. */
						conn->progress->seek = conn->from;
					}
					conn->progress->start = conn->from;
				}
				break;

			default:
				INTERNAL("WHAT???");
		}

		ftp_send_retr_req(conn, connection_state(S_GETH));
		return;
	}

	response = get_ftp_response(conn, rb, 2, NULL);

	if (response == -1) {
		abort_connection(conn, connection_state(S_FTP_ERROR));
		return;
	}

	if (!response) {
		read_from_socket(conn->socket, rb, connection_state(S_GETH),
				 ftp_retr_file);
		return;
	}

	if (response >= 100 && response < 200) {
		/* We only need to parse response after RETR to
		 * get filesize if needed. */
		if (!ftp->dir && conn->est_length == -1) {
			off_t file_len;
			int res;

			file_len = get_filesize_from_RETR(rb->data, rb->length, &res);
			if (file_len > 0) {
				/* FIXME: ..when downloads resuming
				 * implemented.. */
				/* This is right for vsftpd.
				 * Show me urls where this is wrong. --witekfl */
				conn->est_length = res ?
					file_len + conn->progress->start : file_len;
			}
		}
	}

	if (conn->data_socket->fd == -1) {
		/* The passive FTP server did not send a 227 or 229
		 * response.  We check this down here, rather than
		 * immediately after getting the response to the PASV
		 * or EPSV command, to make sure that nothing can
		 * close the socket between the check and the
		 * following set_handlers call.
		 *
		 * If we were using active FTP, then
		 * get_ftp_data_socket would have created the
		 * data_socket without waiting for anything from the
		 * server.  */
		abort_connection(conn, connection_state(S_FTP_ERROR));
		return;
	}
	set_handlers(conn->data_socket->fd, (select_handler_T) ftp_data_accept,
		     NULL, NULL, conn);

	/* read_from_socket(conn->socket, rb, ftp_got_final_response); */
	ftp_got_final_response(socket, rb);
}

static void
ftp_got_final_response(struct socket *socket, struct read_buffer *rb)
{
	struct connection *conn = socket->conn;
	struct ftp_connection_info *ftp = conn->info;
	int response = get_ftp_response(conn, rb, 0, NULL);

	if (response == -1) {
		abort_connection(conn, connection_state(S_FTP_ERROR));
		return;
	}

	if (!response) {
		struct connection_state state = !is_in_state(conn->state, S_TRANS)
			? connection_state(S_GETH) : conn->state;

		read_from_socket(conn->socket, rb, state, ftp_got_final_response);
		return;
	}

	if (response >= 550 || response == 450) {
		/* Requested action not taken.
		 * File unavailable (e.g., file not found, no access). */

		if (!conn->cached)
			conn->cached = get_cache_entry(conn->uri);

		if (!conn->cached
		    || !redirect_cache(conn->cached, "/", 1, 0)) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}

		abort_connection(conn, connection_state(S_OK));
		return;
	}

	if (response >= 400) {
		abort_connection(conn, connection_state(S_FTP_FILE_ERROR));
		return;
	}

	if (ftp->conn_state == 2) {
		ftp_end_request(conn, connection_state(S_OK));
	} else {
		ftp->conn_state = 1;
		if (!is_in_state(conn->state, S_TRANS))
			set_connection_state(conn, connection_state(S_GETH));
	}
}


/** How to format an FTP directory listing in HTML.  */
struct ftp_dir_html_format {
	/** Codepage used by C library functions such as strftime().
	 * If the FTP server sends non-ASCII bytes in file names or
	 * such, ELinks normally passes them straight through to the
	 * generated HTML, which will eventually be parsed using the
	 * codepage specified in the document.codepage.assume option.
	 * However, when ELinks itself generates strings with
	 * strftime(), it turns non-ASCII bytes into entity references
	 * based on libc_codepage, to make sure they become the right
	 * characters again.  */
	int libc_codepage;

	/** Nonzero if directories should be displayed in a different
	 * color.  From the document.browse.links.color_dirs option.  */
	int colorize_dir;

	/** The color of directories, in "#rrggbb" format.  This is
	 * initialized and used only if colorize_dir is nonzero.  */
	unsigned char dircolor[8];
};

/* Display directory entry formatted in HTML. */
static int
display_dir_entry(struct cache_entry *cached, off_t *pos, int *tries,
		  const struct ftp_dir_html_format *format,
		  struct ftp_file_info *ftp_info)
{
	struct string string;
	unsigned char permissions[10] = "---------";

	if (!init_string(&string)) return -1;

	add_char_to_string(&string, ftp_info->type);

	if (ftp_info->permissions) {
		mode_t p = ftp_info->permissions;

#define FTP_PERM(perms, buffer, flag, index, id) \
	if ((perms) & (flag)) (buffer)[(index)] = (id);

		FTP_PERM(p, permissions, S_IRUSR, 0, 'r');
		FTP_PERM(p, permissions, S_IWUSR, 1, 'w');
		FTP_PERM(p, permissions, S_IXUSR, 2, 'x');
		FTP_PERM(p, permissions, S_ISUID, 2, (p & S_IXUSR ? 's' : 'S'));

		FTP_PERM(p, permissions, S_IRGRP, 3, 'r');
		FTP_PERM(p, permissions, S_IWGRP, 4, 'w');
		FTP_PERM(p, permissions, S_IXGRP, 5, 'x');
		FTP_PERM(p, permissions, S_ISGID, 5, (p & S_IXGRP ? 's' : 'S'));

		FTP_PERM(p, permissions, S_IROTH, 6, 'r');
		FTP_PERM(p, permissions, S_IWOTH, 7, 'w');
		FTP_PERM(p, permissions, S_IXOTH, 8, 'x');
		FTP_PERM(p, permissions, S_ISVTX, 8, (p & 0001 ? 't' : 'T'));

#undef FTP_PERM

	}

	add_to_string(&string, permissions);
	add_char_to_string(&string, ' ');

	add_to_string(&string, "   1 ftp      ftp ");

	if (ftp_info->size != FTP_SIZE_UNKNOWN) {
		add_format_to_string(&string, "%12" OFF_PRINT_FORMAT " ",
				     (off_print_T) ftp_info->size);
	} else {
		add_to_string(&string, "           - ");
	}

#ifdef HAVE_STRFTIME
	if (ftp_info->mtime > 0) {
		time_t current_time = time(NULL);
		time_t when = ftp_info->mtime;
		struct tm *when_tm;
	       	unsigned char *fmt;
		/* LC_TIME=fi_FI.UTF_8 can generate "elo___ 31 23:59"
		 * where each _ denotes U+00A0 encoded as 0xC2 0xA0,
		 * thus needing a 19-byte buffer.  */
		unsigned char date[80];
		int wr;

		if (ftp_info->local_time_zone)
			when_tm = localtime(&when);
		else
			when_tm = gmtime(&when);

		if (current_time > when + 6L * 30L * 24L * 60L * 60L
		    || current_time < when - 60L * 60L)
			fmt = "%b %e  %Y";
		else
			fmt = "%b %e %H:%M";

		wr = strftime(date, sizeof(date), fmt, when_tm);
		add_cp_html_to_string(&string, format->libc_codepage,
				      date, wr);
	} else
#endif
	add_to_string(&string, "            ");
	/* TODO: Above, the number of spaces might not match the width
	 * of the string generated by strftime.  It depends on the
	 * locale.  So if ELinks knows the timestamps of some FTP
	 * files but not others, it may misalign the file names.
	 * Potential solutions:
	 * - Pad the strings to a compile-time fixed width.
	 *   Con: If we choose a width that suffices for all known
	 *   locales, then it will be stupidly large for most of them.
	 * - Generate an HTML table.
	 *   Con: Bloats the HTML source.
	 * Any solution chosen here should also be applied to the
	 * file: protocol handler.  */

	add_char_to_string(&string, ' ');

	if (ftp_info->type == FTP_FILE_DIRECTORY && format->colorize_dir) {
		add_to_string(&string, "<font color=\"");
		add_to_string(&string, format->dircolor);
		add_to_string(&string, "\"><b>");
	}

	add_to_string(&string, "<a href=\"");
	encode_uri_string(&string, ftp_info->name.source, ftp_info->name.length, 0);
	if (ftp_info->type == FTP_FILE_DIRECTORY)
		add_char_to_string(&string, '/');
	add_to_string(&string, "\">");
	add_html_to_string(&string, ftp_info->name.source, ftp_info->name.length);
	add_to_string(&string, "</a>");

	if (ftp_info->type == FTP_FILE_DIRECTORY && format->colorize_dir) {
		add_to_string(&string, "</b></font>");
	}

	if (ftp_info->symlink.length) {
		add_to_string(&string, " -&gt; ");
		add_html_to_string(&string, ftp_info->symlink.source,
				ftp_info->symlink.length);
	}

	add_char_to_string(&string, '\n');

	if (add_fragment(cached, *pos, string.source, string.length)) *tries = 0;
	*pos += string.length;

	done_string(&string);
	return 0;
}

/* Get the next line of input and set *@len to the length of the line.
 * Return the number of newline characters at the end of the line or -1
 * if we must wait for more input. */
static int
ftp_get_line(struct cache_entry *cached, unsigned char *buf, int bufl,
             int last, int *len)
{
	unsigned char *newline;

	if (!bufl) return -1;

	newline = memchr(buf, ASCII_LF, bufl);

	if (newline) {
		*len = newline - buf;
		if (*len && buf[*len - 1] == ASCII_CR) {
			--*len;
			return 2;
		} else {
			return 1;
		}
	}

	if (last || bufl >= FTP_BUF_SIZE) {
		*len = bufl;
		return 0;
	}

	return -1;
}

/** Generate HTML for a line that was received from the FTP server but
 * could not be parsed.  The caller is supposed to have added a \<pre>
 * start tag.  (At the time of writing, init_directory_listing() was
 * used for that.)
 *
 * @return -1 if out of memory, or 0 if successful.  */
static int
ftp_add_unparsed_line(struct cache_entry *cached, off_t *pos, int *tries, 
		      const unsigned char *line, int line_length)
{
	int our_ret;
	struct string string;
	int frag_ret;

	our_ret = -1;	 /* assume out of memory if returning early */
	if (!init_string(&string)) goto out;
	if (!add_html_to_string(&string, line, line_length)) goto out;
	if (!add_char_to_string(&string, '\n')) goto out;

	frag_ret = add_fragment(cached, *pos, string.source, string.length);
	if (frag_ret == -1) goto out;
	*pos += string.length;
	if (frag_ret == 1) *tries = 0;

	our_ret = 0;		/* success */

out:
	done_string(&string);	/* safe even if init_string failed */
	return our_ret;
}

/** List a directory in html format.
 *
 * @return the number of bytes used from the beginning of @a buffer,
 * or -1 if out of memory.  */
static int
ftp_process_dirlist(struct cache_entry *cached, off_t *pos,
		    unsigned char *buffer, int buflen, int last,
		    int *tries, const struct ftp_dir_html_format *format)
{
	int ret = 0;

	while (1) {
		struct ftp_file_info ftp_info = INIT_FTP_FILE_INFO;
		unsigned char *buf = buffer + ret;
		int bufl = buflen - ret;
		int line_length, eol;

		eol = ftp_get_line(cached, buf, bufl, last, &line_length);
		if (eol == -1)
			return ret;

		ret += line_length + eol;

		/* Process line whose end we've already found. */

		if (parse_ftp_file_info(&ftp_info, buf, line_length)) {
			int retv;

			if (ftp_info.name.source[0] == '.'
			    && (ftp_info.name.length == 1
				|| (ftp_info.name.length == 2
				    && ftp_info.name.source[1] == '.')))
				continue;

			retv = display_dir_entry(cached, pos, tries,
						 format, &ftp_info);
			if (retv < 0) {
				return ret;
			}

		} else {
			int retv = ftp_add_unparsed_line(cached, pos, tries,
							 buf, line_length);

			if (retv == -1) /* out of memory; propagate to caller */
				return retv;
		}
	}
}

/* This is the initial read handler for conn->data_socket->fd,
 * which may be either trying to connect to a passive FTP server or
 * listening for a connection from an active FTP server.  In active
 * FTP, this function then accepts the connection and replaces
 * conn->data_socket->fd with the resulting socket.  In any case,
 * this function does not read any data from the FTP server, but
 * rather hands the socket over to got_something_from_data_connection,
 * which then does the reads.  */
static void
ftp_data_accept(struct connection *conn)
{
	struct ftp_connection_info *ftp = conn->info;
	int newsock;

	/* Because this function is called only as a read handler of
	 * conn->data_socket->fd, the socket must be valid if we get
	 * here.  */
	assert(conn->data_socket->fd >= 0);
	if_assert_failed return;

	set_connection_timeout(conn);
	clear_handlers(conn->data_socket->fd);

	if ((conn->socket->protocol_family != EL_PF_INET6 && ftp->use_pasv)
#ifdef CONFIG_IPV6
	    || (conn->socket->protocol_family == EL_PF_INET6 && ftp->use_epsv)
#endif
	   ) {
		newsock = conn->data_socket->fd;
	} else {
		newsock = accept(conn->data_socket->fd, NULL, NULL);
		if (newsock < 0) {
			retry_connection(conn, connection_state_for_errno(errno));
			return;
		}
		close(conn->data_socket->fd);
	}

	conn->data_socket->fd = newsock;

	set_handlers(newsock,
		     (select_handler_T) got_something_from_data_connection,
		     NULL, NULL, conn);
}

/* A read handler for conn->data_socket->fd.  This function reads
 * data from the FTP server, reformats it to HTML if it's a directory
 * listing, and adds the result to the cache entry.  */
static void
got_something_from_data_connection(struct connection *conn)
{
	struct ftp_connection_info *ftp = conn->info;
	struct ftp_dir_html_format format;
	ssize_t len;

	/* Because this function is called only as a read handler of
	 * conn->data_socket->fd, the socket must be valid if we get
	 * here.  */
	assert(conn->data_socket->fd >= 0);
	if_assert_failed return;

	/* XXX: This probably belongs rather to connect.c ? */

	set_connection_timeout(conn);

	if (!conn->cached) conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
out_of_mem:
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (ftp->dir) {
		format.libc_codepage = get_cp_index("System");

		format.colorize_dir = get_opt_bool("document.browse.links.color_dirs");

		if (format.colorize_dir) {
			color_to_string(get_opt_color("document.colors.dirs"),
					format.dircolor);
		}
	}

	if (ftp->dir && !conn->from) {
		struct string string;
		struct connection_state state;

		if (!conn->uri->data) {
			abort_connection(conn, connection_state(S_FTP_ERROR));
			return;
		}

		state = init_directory_listing(&string, conn->uri);
		if (!is_in_state(state, S_OK)) {
			abort_connection(conn, state);
			return;
		}

		add_fragment(conn->cached, conn->from, string.source, string.length);
		conn->from += string.length;

		done_string(&string);

		if (conn->uri->datalen) {
			struct ftp_file_info ftp_info = INIT_FTP_FILE_INFO_ROOT;

			display_dir_entry(conn->cached, &conn->from, &conn->tries,
					  &format, &ftp_info);
		}

		mem_free_set(&conn->cached->content_type, stracpy("text/html"));
	}

	len = safe_read(conn->data_socket->fd, ftp->ftp_buffer + ftp->buf_pos,
		        FTP_BUF_SIZE - ftp->buf_pos);
	if (len < 0) {
		retry_connection(conn, connection_state_for_errno(errno));
		return;
	}

	if (len > 0) {
		conn->received += len;

		if (!ftp->dir) {
			if (add_fragment(conn->cached, conn->from,
					 ftp->ftp_buffer, len) == 1)
				conn->tries = 0;
			conn->from += len;

		} else {
			int proceeded;

			proceeded = ftp_process_dirlist(conn->cached,
							&conn->from,
							ftp->ftp_buffer,
							len + ftp->buf_pos,
							0, &conn->tries,
							&format);

			if (proceeded == -1) goto out_of_mem;

			ftp->buf_pos += len - proceeded;

			memmove(ftp->ftp_buffer, ftp->ftp_buffer + proceeded,
				ftp->buf_pos);

		}

		set_connection_state(conn, connection_state(S_TRANS));
		return;
	}

	if (ftp_process_dirlist(conn->cached, &conn->from,
				ftp->ftp_buffer, ftp->buf_pos, 1,
				&conn->tries, &format) == -1)
		goto out_of_mem;

#define ADD_CONST(str) { \
	add_fragment(conn->cached, conn->from, str, sizeof(str) - 1); \
	conn->from += (sizeof(str) - 1); }

	if (ftp->dir) ADD_CONST("</pre>\n<hr/>\n</body>\n</html>");

	close_socket(conn->data_socket);

	if (ftp->conn_state == 1) {
		ftp_end_request(conn, connection_state(S_OK));
	} else {
		ftp->conn_state = 2;
		set_connection_state(conn, connection_state(S_TRANS));
	}
}

static void
ftp_end_request(struct connection *conn, struct connection_state state)
{
	if (is_in_state(state, S_OK) && conn->cached) {
		normalize_cache_entry(conn->cached, conn->from);
	}

	set_connection_state(conn, state);
	add_keepalive_connection(conn, FTP_KEEPALIVE_TIMEOUT, NULL);
}
