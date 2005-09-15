/* Internal "ftp" protocol implementation */
/* $Id: ftp.c,v 1.182.2.8 2005/09/14 11:36:12 jonas Exp $ */

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
#include "lowlevel/connect.h"
#include "lowlevel/select.h"
#include "modules/module.h"
#include "osdep/osdep.h"
#include "osdep/stat.h"
#include "protocol/auth/auth.h"
#include "protocol/ftp/ftp.h"
#include "protocol/ftp/parse.h"
#include "protocol/uri.h"
#include "sched/connection.h"
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
		N_("Host and port-number (host:port) of the FTP proxy, or blank.\n"
		"If it's blank, FTP_PROXY environment variable is checked as well.")),

	INIT_OPT_STRING("protocol.ftp", N_("Anonymous password"),
		"anon_passwd", 0, "some@host.domain",
		N_("FTP anonymous password to be sent.")),

	INIT_OPT_BOOL("protocol.ftp", N_("Use passive mode (IPv4)"),
		"use_pasv", 0, 1,
		N_("Use PASV instead of PORT (passive vs active mode, IPv4 only).")),
#ifdef CONFIG_IPV6
	INIT_OPT_BOOL("protocol.ftp", N_("Use passive mode (IPv6)"),
		"use_epsv", 0, 0,
		N_("Use EPSV instead of EPRT (passive vs active mode, IPv6 only).")),
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
	unsigned int has_data:1;     /* Do we have data socket? */
	unsigned int use_pasv:1;     /* Use PASV (yes or no) */
#ifdef CONFIG_IPV6
	unsigned int use_epsv:1;     /* Use EPSV */
#endif
	unsigned char ftp_buffer[FTP_BUF_SIZE];
	unsigned char cmd_buffer[1]; /* Must be last field !! */
};


/* Prototypes */
static void ftp_login(struct connection *);
static void ftp_send_retr_req(struct connection *, int);
static void ftp_got_info(struct connection *, struct read_buffer *);
static void ftp_got_user_info(struct connection *, struct read_buffer *);
static void ftp_pass(struct connection *);
static void ftp_pass_info(struct connection *, struct read_buffer *);
static void ftp_retr_file(struct connection *, struct read_buffer *);
static void ftp_got_final_response(struct connection *, struct read_buffer *);
static void got_something_from_data_connection(struct connection *);
static void ftp_end_request(struct connection *, int);
static struct ftp_connection_info *add_file_cmd_to_str(struct connection *);

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
	int pos;

	set_connection_timeout(conn);

again:
	for (pos = 0; pos < rb->len; pos++) {
		unsigned char *num_end;
		int response;

		if (rb->data[pos] != ASCII_LF) continue;

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
			if (getpeername(conn->socket.fd, (struct sockaddr *) sa, &sal)) {
				return -1;
			}
			s->sin6_family = AF_INET6;
			s->sin6_port = htons(n[5]);
		}
#endif

		if (*num_end == '-') {
			int i;

			for (i = 0; i < rb->len - 5; i++)
				if (rb->data[i] == ASCII_LF
				    && !memcmp(rb->data+i+1, rb->data, 3)
				    && rb->data[i+4] == ' ') {
					for (i++; i < rb->len; i++)
						if (rb->data[i] == ASCII_LF)
							goto ok;
					return 0;
				}
			return 0;
ok:
			pos = i;
		}

		if (!part && response >= 100 && response < 200) {
			kill_buffer_data(rb, pos + 1);
			goto again;
		}

		if (part == 2)
			return response;

		kill_buffer_data(rb, pos + 1);
		return response;
	}

	return 0;
}


/* Initialize or continue ftp connection. */
void
ftp_protocol_handler(struct connection *conn)
{
	set_connection_timeout(conn);

	if (!has_keepalive_connection(conn)) {
		make_connection(conn, &conn->socket, ftp_login);

	} else {
		ftp_send_retr_req(conn, S_SENT);
	}
}

/* Get connection response. */
static void
get_resp(struct connection *conn)
{
	struct read_buffer *rb = alloc_read_buffer(conn);

	if (!rb) return;
	read_from_socket(conn, &conn->socket, rb, conn->read_func);
}

/* Send command, set connection state and free cmd string. */
static void
send_cmd(struct connection *conn, struct string *cmd, void *callback, int state)
{
	conn->read_func = callback;
	write_to_socket(conn, &conn->socket, cmd->source, cmd->length, get_resp);

	done_string(cmd);
	set_connection_state(conn, state);
}

/* Check if this auth token really belongs to this URI. */
static int
auth_user_matching_uri(struct auth_entry *auth, struct uri *uri)
{
	if (!uri->userlen) /* Noone said it doesn't. */
		return 1;
	return !strlcasecmp(auth->user, -1, uri->user, uri->userlen);
}


/* Kill the current connection and ask for a username/password for the next
 * try. */
static void
prompt_username_pw(struct connection *conn)
{
	if (!conn->cached) {
		conn->cached = get_cache_entry(conn->uri);
		if (!conn->cached) {
			abort_conn_with_state(conn, S_OUT_OF_MEM);
			return;
		}
	}

	mem_free_set(&conn->cached->content_type, stracpy("text/html"));
	if (!conn->cached->content_type) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	add_auth_entry(conn->uri, "FTP Login", NULL, NULL, 0);

	abort_conn_with_state(conn, S_OK);
}

/* Send USER command. */
static void
ftp_login(struct connection *conn)
{
	struct string cmd;
	struct auth_entry* auth;

	auth = find_auth(conn->uri);

	if (!init_string(&cmd)) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
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

	send_cmd(conn, &cmd, (void *) ftp_got_info, S_SENT);
}

/* Parse connection response. */
static void
ftp_got_info(struct connection *conn, struct read_buffer *rb)
{
	int response = get_ftp_response(conn, rb, 0, NULL);

	if (response == -1) {
		abort_conn_with_state(conn, S_FTP_ERROR);
		return;
	}

	if (!response) {
		read_from_socket(conn, &conn->socket, rb, ftp_got_info);
		return;
	}

	/* RFC959 says that possible response codes on connection are:
	 * 120 Service ready in nnn minutes.
	 * 220 Service ready for new user.
	 * 421 Service not available, closing control connection. */

	if (response != 220) {
		/* TODO? Retry in case of ... ?? */
		retry_conn_with_state(conn, S_FTP_UNAVAIL);
		return;
	}

	ftp_got_user_info(conn, rb);
}


/* Parse USER response and send PASS command if needed. */
static void
ftp_got_user_info(struct connection *conn, struct read_buffer *rb)
{
	int response = get_ftp_response(conn, rb, 0, NULL);

	if (response == -1) {
		abort_conn_with_state(conn, S_FTP_ERROR);
		return;
	}

	if (!response) {
		read_from_socket(conn, &conn->socket, rb, ftp_got_user_info);
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
		abort_conn_with_state(conn, S_FTP_UNAVAIL);
		return;
	}

	if (response == 230) {
		ftp_send_retr_req(conn, S_GETH);
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
		abort_conn_with_state(conn, S_OUT_OF_MEM);
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

	send_cmd(conn, &cmd, (void *) ftp_pass_info, S_LOGIN);
}

/* Parse PASS command response. */
static void
ftp_pass_info(struct connection *conn, struct read_buffer *rb)
{
	int response = get_ftp_response(conn, rb, 0, NULL);

	if (response == -1) {
		abort_conn_with_state(conn, S_FTP_ERROR);
		return;
	}

	if (!response) {
		read_from_socket(conn, &conn->socket, rb, ftp_pass_info);
		set_connection_state(conn, S_LOGIN);
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
		abort_conn_with_state(conn, S_FTP_UNAVAIL);
		return;
	}

	ftp_send_retr_req(conn, S_GETH);
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

/* Create passive socket and add appropriate announcing commands to str. Then
 * go and retrieve appropriate object from server.
 * Returns NULL if error. */
static struct ftp_connection_info *
add_file_cmd_to_str(struct connection *conn)
{
#ifdef CONFIG_IPV6
	struct sockaddr_storage data_addr;
#endif
	struct ftp_connection_info *c_i;
	struct string command;
	int data_sock;
	unsigned char pc[6];

	c_i = mem_calloc(1, sizeof(*c_i));
	if (!c_i) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return NULL;
	}

	if (!init_string(&command)) {
		mem_free(c_i);
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return NULL;
	}

	conn->info = c_i;

#ifdef CONFIG_IPV6
	memset(&data_addr, 0, sizeof(data_addr));
#endif
	memset(pc, 0, 6);

	if (get_opt_bool("protocol.ftp.use_pasv"))
		c_i->use_pasv = 1;

#ifdef CONFIG_IPV6
	if (get_opt_bool("protocol.ftp.use_epsv"))
		c_i->use_epsv = 1;

	if (!c_i->use_epsv && conn->protocol_family == 1) {
		data_sock = get_pasv6_socket(conn, conn->socket.fd, &data_addr);
		if (data_sock < 0)
			return NULL;
		conn->data_socket.fd = data_sock;
	}
#endif

	if (!c_i->use_pasv && conn->protocol_family != 1) {
		data_sock = get_pasv_socket(conn, conn->socket.fd, pc);
		if (data_sock < 0)
			return NULL;
		conn->data_socket.fd = data_sock;
	}

	if (!conn->uri->data) {
		INTERNAL("conn->uri->data empty");
		abort_conn_with_state(conn, S_INTERNAL);
		return NULL;
	}

	if (!conn->uri->datalen
	    || conn->uri->data[conn->uri->datalen - 1] == '/') {
		/* Commands to get directory listing. */

		c_i->dir = 1;
		c_i->pending_commands = 4;

		/* ASCII */
		add_to_string(&command, "TYPE A");
		add_crlf_to_string(&command);

#ifdef CONFIG_IPV6
		if (conn->protocol_family == 1)
			if (c_i->use_epsv)
				add_to_string(&command, "EPSV");
			else
				add_eprtcmd_to_string(&command,
					(struct sockaddr_in6 *) &data_addr);
		else
#endif
			if (c_i->use_pasv)
				add_to_string(&command, "PASV");
			else
				add_portcmd_to_string(&command, pc);

		add_crlf_to_string(&command);

		add_to_string(&command, "CWD ");
		add_uri_to_string(&command, conn->uri, URI_PATH);
		add_crlf_to_string(&command);

		add_to_string(&command, "LIST");
		add_crlf_to_string(&command);

		conn->from = 0;

	} else {
		/* Commands to get a file. */

		c_i->dir = 0;
		c_i->pending_commands = 3;

		/* BINARY */
		add_to_string(&command, "TYPE I");
		add_crlf_to_string(&command);

#ifdef CONFIG_IPV6
		if (conn->protocol_family == 1)
			if (c_i->use_epsv)
				add_to_string(&command, "EPSV");
			else
				add_eprtcmd_to_string(&command,
					(struct sockaddr_in6 *) &data_addr);
		else
#endif
			if (c_i->use_pasv)
				add_to_string(&command, "PASV");
			else
				add_portcmd_to_string(&command, pc);

		add_crlf_to_string(&command);

		if (conn->from || (conn->progress.start > 0)) {
			add_to_string(&command, "REST ");
			add_long_to_string(&command, conn->from
							? conn->from
							: conn->progress.start);
			add_crlf_to_string(&command);

			c_i->rest_sent = 1;
			c_i->pending_commands++;
		}

		add_to_string(&command, "RETR ");
		add_uri_to_string(&command, conn->uri, URI_PATH);
		add_crlf_to_string(&command);
	}

	c_i->opc = c_i->pending_commands;

	/* 1 byte is already reserved for cmd_buffer in struct ftp_connection_info. */
	c_i = mem_realloc(c_i, sizeof(*c_i) + command.length);
	if (!c_i) {
		done_string(&command);
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return NULL;
	}

	memcpy(c_i->cmd_buffer, command.source, command.length + 1);
	done_string(&command);
	conn->info = c_i;

	return c_i;
}

static void
send_it_line_by_line(struct connection *conn, struct string *cmd)
{
	struct ftp_connection_info *c_i = conn->info;
	unsigned char *nl = strchr(c_i->cmd_buffer, '\n');

	if (!nl) {
		add_to_string(cmd, c_i->cmd_buffer);
		return;
	}

	nl++;
	add_bytes_to_string(cmd, c_i->cmd_buffer, nl - c_i->cmd_buffer);
	memmove(c_i->cmd_buffer, nl, strlen(nl) + 1);
}

/* Send commands to retrieve file or directory. */
static void
ftp_send_retr_req(struct connection *conn, int state)
{
	struct string cmd;

	if (!init_string(&cmd)) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
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
static long int
get_filesize_from_RETR(unsigned char *data, int data_len)
{
	long int file_len;
	int pos;
	int pos_file_len = 0;

	/* Getting file size from text response.. */
	/* 150 Opening BINARY mode data connection for hello-1.0-1.1.diff.gz (16452 bytes). */

	for (pos = 0; pos < data_len && data[pos] != ASCII_LF; pos++)
		if (data[pos] == '(')
			pos_file_len = pos;

	if (!pos_file_len || pos_file_len == data_len - 1)
		return -1;

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

	if (strncasecmp(&data[pos], "byte", 4))
		return -1;

	errno = 0;
	file_len = strtol(&data[pos_file_len], NULL, 10);
	if (errno) return -1;

	return file_len;
}

static int
ftp_data_connect(struct connection *conn, int family, struct sockaddr_storage *sa,
		 int size_of_sockaddr)
{
	int fd = socket(family, SOCK_STREAM, 0);

	if (fd < 0 || set_nonblocking_fd(fd) < 0) {
		abort_conn_with_state(conn, S_FTP_ERROR);
		return -1;
	}

	set_ip_tos_throughput(fd);

	conn->data_socket.fd = fd;
	/* XXX: We ignore connect() errors here. */
	connect(fd, (struct sockaddr *) sa, size_of_sockaddr);
	return 0;
}

static void
ftp_retr_file(struct connection *conn, struct read_buffer *rb)
{
	struct ftp_connection_info *c_i = conn->info;
	struct sockaddr_storage sa;
	int response;

	if (c_i->pending_commands > 1) {
		response = get_ftp_response(conn, rb, 0, &sa);

		if (response == -1) {
			abort_conn_with_state(conn, S_FTP_ERROR);
			return;
		}

		if (!response) {
			read_from_socket(conn, &conn->socket, rb, ftp_retr_file);
			set_connection_state(conn, S_GETH);
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

		c_i->pending_commands--;

		/* XXX: The case values are order numbers of commands. */
		switch (c_i->opc - c_i->pending_commands) {
			case 1:	/* TYPE */
				break;

			case 2:	/* PORT */
				if (response >= 400) {
					abort_conn_with_state(conn, S_FTP_PORT);
					return;
				}
				break;

			case 3:	/* REST / CWD */
				if (response >= 400) {
					if (c_i->dir) {
						abort_conn_with_state(conn,
								S_FTP_NO_FILE);
						return;
					}
					conn->from = 0;
				} else if (c_i->rest_sent) {
					/* Following code is related to resume
					 * feature. */
					if (response == 350)
						conn->from = conn->progress.start;
					/* Come on, don't be nervous ;-). */
					if (conn->progress.start >= 0) {
						/* Update to the real value
						 * which we've got from
						 * Content-Range. */
						conn->progress.seek = conn->from;
					}
					conn->progress.start = conn->from;
				}
				break;

			default:
				INTERNAL("WHAT???");
		}

		ftp_send_retr_req(conn, S_GETH);
		return;
	}

	response = get_ftp_response(conn, rb, 2, NULL);

	if (response == -1) {
		abort_conn_with_state(conn, S_FTP_ERROR);
		return;
	}

	if (!response) {
		read_from_socket(conn, &conn->socket, rb, ftp_retr_file);
		set_connection_state(conn, S_GETH);
		return;
	}

	if (response >= 100 && response < 200) {
		/* We only need to parse response after RETR to
		 * get filesize if needed. */
		if (!c_i->dir && conn->est_length == -1) {
			long int file_len =
				get_filesize_from_RETR(rb->data, rb->len);

			if (file_len > 0) {
				/* FIXME: ..when downloads resuming
				 * implemented.. */
				conn->est_length = file_len + conn->progress.start;
			}
		}
	}

	set_handlers(conn->data_socket.fd,
		     (void (*)(void *)) got_something_from_data_connection,
		     NULL, NULL, conn);

	/* read_from_socket(conn, &conn->socket, rb, ftp_got_final_response); */
	ftp_got_final_response(conn, rb);
}

static void
ftp_got_final_response(struct connection *conn, struct read_buffer *rb)
{
	struct ftp_connection_info *c_i = conn->info;
	int response = get_ftp_response(conn, rb, 0, NULL);

	if (response == -1) {
		abort_conn_with_state(conn, S_FTP_ERROR);
		return;
	}

	if (!response) {
		read_from_socket(conn, &conn->socket, rb, ftp_got_final_response);
		if (conn->state != S_TRANS)
			set_connection_state(conn, S_GETH);
		return;
	}

	if (response >= 550 || response == 450) {
		/* Requested action not taken.
		 * File unavailable (e.g., file not found, no access). */

		if (!conn->cached)
			conn->cached = get_cache_entry(conn->uri);

		if (!conn->cached
		    || !redirect_cache(conn->cached, "/", 1, 0)) {
			abort_conn_with_state(conn, S_OUT_OF_MEM);
			return;
		}

		abort_conn_with_state(conn, S_OK);
		return;
	}

	if (response >= 400) {
		abort_conn_with_state(conn, S_FTP_FILE_ERROR);
		return;
	}

	if (c_i->conn_state == 2) {
		ftp_end_request(conn, S_OK);
	} else {
		c_i->conn_state = 1;
		if (conn->state != S_TRANS)
			set_connection_state(conn, S_GETH);
	}
}


/* Display directory entry formatted in HTML. */
static int
display_dir_entry(struct cache_entry *cached, int *pos, int *tries,
		  int colorize_dir, unsigned char *dircolor,
		  struct ftp_file_info *ftp_info)
{
	struct string string;
	unsigned char permissions[10] = "---------";

	if (!init_string(&string)) return -1;

	add_char_to_string(&string, ftp_info->type);

	if (ftp_info->permissions) {
		int p = ftp_info->permissions;

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
		add_format_to_string(&string, "%12lu ", ftp_info->size);
	} else {
		add_to_string(&string, "           - ");
	}

#ifdef HAVE_STRFTIME
	if (ftp_info->mtime > 0) {
		time_t current_time = time(NULL);
		time_t when = ftp_info->mtime;
		struct tm *when_tm;
	       	unsigned char *fmt;
		unsigned char date[13];
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

		while (wr < sizeof(date) - 1) date[wr++] = ' ';
		date[sizeof(date) - 1] = '\0';
		add_to_string(&string, date);
	} else
#endif
	add_to_string(&string, "            ");

	add_char_to_string(&string, ' ');

	if (ftp_info->type == FTP_FILE_DIRECTORY && colorize_dir) {
		add_to_string(&string, "<font color=\"");
		add_to_string(&string, dircolor);
		add_to_string(&string, "\"><b>");
	}

	add_to_string(&string, "<a href=\"");
	add_html_to_string(&string, ftp_info->name.source, ftp_info->name.length);
	if (ftp_info->type == FTP_FILE_DIRECTORY)
		add_char_to_string(&string, '/');
	add_to_string(&string, "\">");
	add_html_to_string(&string, ftp_info->name.source, ftp_info->name.length);
	add_to_string(&string, "</a>");

	if (ftp_info->type == FTP_FILE_DIRECTORY && colorize_dir) {
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

/* List a directory in html format. */
static int
ftp_process_dirlist(struct cache_entry *cached, int *pos,
		    unsigned char *buffer, int buflen, int last,
		    int *tries, int colorize_dir, unsigned char *dircolor)
{
	int ret = 0;
#ifdef DEBUG_FTP_PARSER
 	static int debug_ftp_parser = 1;
	int buflen_orig = buflen;
	unsigned char *response_orig = NULL;

	if (debug_ftp_parser) {
		buffer = get_ftp_debug_parse_responses(buffer, buflen);
		buflen = strlen(buffer);
		response_orig = buffer;
		debug_ftp_parser = 0;
	}

#define	end_ftp_dirlist_processing()	do { mem_free_if(response_orig); } while (0)
#define get_ftp_dirlist_offset(retval)	int_min(retval, buflen_orig)
#else
#define	end_ftp_dirlist_processing()	/* Nothing to free */
#define	get_ftp_dirlist_offset(retval)	(retval)
#endif

	while (1) {
		struct ftp_file_info ftp_info = INIT_FTP_FILE_INFO;
		unsigned char *buf = buffer + ret;
		int bufl = buflen - ret;
		int bufp;
		int newline = 0;

		/* Newline quest. */

		for (bufp = 0; bufp < bufl; bufp++) {
			if (buf[bufp] == ASCII_LF) {
				newline = 1;
				break;
			}
		}

		if (newline) {
			ret += bufp + 1;
			if (bufp && buf[bufp - 1] == ASCII_CR) bufp--;
		} else {
			if (!bufp || (!last && bufl < FTP_BUF_SIZE)) {
				end_ftp_dirlist_processing();
				return get_ftp_dirlist_offset(ret);
			}

			ret += bufp;
		}

		/* Process line whose end we've already found. */

		if (parse_ftp_file_info(&ftp_info, buf, bufp)) {
			int retv;

			if ((ftp_info.name.length == 1 && ftp_info.name.source[0] == '.')
			    || (ftp_info.name.length == 2 && ftp_info.name.source[0] == '.'
				&& ftp_info.name.source[1] == '.'))
				continue;

			retv = display_dir_entry(cached, pos, tries, colorize_dir,
						dircolor, &ftp_info);
			if (retv < 0) {
				end_ftp_dirlist_processing();
				return get_ftp_dirlist_offset(ret);
			}
		}
#ifdef DEBUG_FTP_PARSER
		else {
			ERROR("Error parsing: [%.*s]", bufp, buf);
		}
#endif
	}
}

static int
ftp_data_accept(struct connection *conn)
{
	struct ftp_connection_info *c_i = conn->info;
	int newsock;

	if (c_i->has_data) return 0;

	c_i->has_data = 1;

	clear_handlers(conn->data_socket.fd);

	if ((conn->protocol_family != 1 && c_i->use_pasv)
#ifdef CONFIG_IPV6
	    || (conn->protocol_family == 1 && c_i->use_epsv)
#endif
	   ) {
		newsock = conn->data_socket.fd;
	} else {
		newsock = accept(conn->data_socket.fd, NULL, NULL);
		if (newsock < 0) {
			retry_conn_with_state(conn, -errno);
			return -1;
		}
		close(conn->data_socket.fd);
	}

	conn->data_socket.fd = newsock;

	set_handlers(newsock,
		     (void (*)(void *)) got_something_from_data_connection,
		     NULL, NULL, conn);
	return 0;
}

static void
got_something_from_data_connection(struct connection *conn)
{
	struct ftp_connection_info *c_i = conn->info;
	unsigned char dircolor[8];
	int colorize_dir = 0;
	int len;

	/* XXX: This probably belongs rather to connect.c ? */

	set_connection_timeout(conn);

	if (ftp_data_accept(conn)) return;

	if (!conn->cached) conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
out_of_mem:
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	if (c_i->dir) {
		colorize_dir = get_opt_bool("document.browse.links.color_dirs");

		if (colorize_dir) {
			color_to_string(get_opt_color("document.colors.dirs"),
					(unsigned char *) &dircolor);
		}
	}

	if (c_i->dir && !conn->from) {
		struct string string;
		unsigned char *uristring;

		if (!conn->uri->data) {
			abort_conn_with_state(conn, S_FTP_ERROR);
			return;
		}

		uristring = get_uri_string(conn->uri, URI_PUBLIC);
		if (!uristring) goto out_of_mem;

		if (!init_string(&string)) {
			mem_free(uristring);
			goto out_of_mem;
		}

		add_html_to_string(&string, uristring, strlen(uristring));
		mem_free(uristring);

#define ADD_CONST(str) { \
	add_fragment(conn->cached, conn->from, str, sizeof(str) - 1); \
	conn->from += (sizeof(str) - 1); }

#define ADD_STRING() { \
	add_fragment(conn->cached, conn->from, string.source, string.length); \
	conn->from += string.length; }

		ADD_CONST("<html>\n<head><title>");
		ADD_STRING();
		ADD_CONST("</title></head>\n<body>\n<h2>FTP directory ");
		ADD_STRING();
		ADD_CONST("</h2>\n<pre>");

		done_string(&string);

		if (conn->uri->datalen) {
			struct ftp_file_info ftp_info = INIT_FTP_FILE_INFO_ROOT;

			display_dir_entry(conn->cached, &conn->from, &conn->tries,
					  colorize_dir, dircolor, &ftp_info);
		}

		mem_free_set(&conn->cached->content_type, stracpy("text/html"));
	}

	len = safe_read(conn->data_socket.fd, c_i->ftp_buffer + c_i->buf_pos,
		        FTP_BUF_SIZE - c_i->buf_pos);
	if (len < 0) {
		retry_conn_with_state(conn, -errno);
		return;
	}

	if (len > 0) {
		if (!c_i->dir) {
			conn->received += len;
			if (add_fragment(conn->cached, conn->from,
					 c_i->ftp_buffer, len) == 1)
				conn->tries = 0;
			conn->from += len;

		} else {
			int proceeded;

			conn->received += len;
			proceeded = ftp_process_dirlist(conn->cached,
							&conn->from,
							c_i->ftp_buffer,
							len + c_i->buf_pos,
							0, &conn->tries,
							colorize_dir,
							(unsigned char *) dircolor);

			if (proceeded == -1) goto out_of_mem;

			c_i->buf_pos += len - proceeded;

			memmove(c_i->ftp_buffer, c_i->ftp_buffer + proceeded,
				c_i->buf_pos);

		}

		set_connection_state(conn, S_TRANS);
		return;
	}

	if (ftp_process_dirlist(conn->cached, &conn->from,
				c_i->ftp_buffer, c_i->buf_pos, 1,
				&conn->tries, colorize_dir,
				(unsigned char *) dircolor) == -1)
		goto out_of_mem;

	if (c_i->dir) ADD_CONST("</pre>\n<hr>\n</body>\n</html>");

	clear_handlers(conn->data_socket.fd);
	close_socket(NULL, &conn->data_socket);

	if (c_i->conn_state == 1) {
		ftp_end_request(conn, S_OK);
	} else {
		c_i->conn_state = 2;
		set_connection_state(conn, S_TRANS);
	}
}

static void
ftp_end_request(struct connection *conn, enum connection_state state)
{
	set_connection_state(conn, state);

	if (conn->state == S_OK && conn->cached) {
		truncate_entry(conn->cached, conn->from, 1);
		conn->cached->incomplete = 0;
	}

	add_keepalive_connection(conn, FTP_KEEPALIVE_TIMEOUT, NULL);
}
