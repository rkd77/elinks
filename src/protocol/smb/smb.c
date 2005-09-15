/* Internal SMB protocol implementation */
/* $Id: smb.c,v 1.78 2005/06/13 00:43:29 jonas Exp $ */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for asprintf() */
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h> /* FreeBSD needs this before resource.h */
#endif
#include <sys/types.h> /* FreeBSD needs this before resource.h */
#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "main/select.h"
#include "network/connection.h"
#include "network/socket.h"
#include "osdep/osdep.h"
#include "protocol/protocol.h"
#include "protocol/smb/smb.h"
#include "protocol/uri.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"

/* XXX: Nice cleanup target --pasky */
/* FIXME: we rely on smbclient output which may change in future,
 * so i think we should use libsmbclient instead (or better in addition)
 * This stuff is a quick hack, but it works ;). --Zas */

enum smb_list_type {
	SMB_LIST_NONE,
	SMB_LIST_SHARES,
	SMB_LIST_DIR,
};

struct smb_connection_info {
	enum smb_list_type list_type;

	/* If this is 1, it means one socket is already off. The second one
	 * should call end_smb_connection() when it goes off as well. */
	int closing;

	size_t textlen;
	unsigned char text[1];
};

static void end_smb_connection(struct connection *conn);


struct option_info smb_options[] = {
	INIT_OPT_TREE("protocol", N_("SMB"),
		"smb", 0,
		N_("SAMBA specific options.")),

	INIT_OPT_STRING("protocol.smb", N_("Credentials"),
		"credentials", 0, "",
		N_("Credentials file passed to smbclient via -A option.")),

	NULL_OPTION_INFO,
};

struct module smb_protocol_module = struct_module(
	/* name: */		N_("SMB"),
	/* options: */		smb_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);


/* Return 0 if @conn->cached was set. */
static int
smb_get_cache(struct connection *conn)
{
	if (conn->cached) return 0;

	conn->cached = get_cache_entry(conn->uri);
	if (conn->cached) return 0;

	abort_connection(conn, S_OUT_OF_MEM);
	return -1;
}


#define READ_SIZE	4096

static ssize_t
smb_read_data(struct connection *conn, int sock, unsigned char *dst)
{
	ssize_t r;
	struct smb_connection_info *si = conn->info;

	r = read(sock, dst, READ_SIZE);
	if (r == -1) {
		retry_connection(conn, -errno);
		return -1;
	}
	if (r == 0) {
		if (!si->closing) {
			si->closing = 1;
			clear_handlers(sock);
			return 0;
		}
		end_smb_connection(conn);
		return 0;
	}

	return r;
}

static void
smb_read_text(struct connection *conn, int sock)
{
	ssize_t r;
	struct smb_connection_info *si = conn->info;

	/* We add 2 here to handle LF and NUL chars that are added in
	 * smb_end_connection(). */
	si = mem_realloc(si, sizeof(*si) + si->textlen
			     + READ_SIZE + 2);
	if (!si) {
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}
	conn->info = si;

	r = smb_read_data(conn, sock, si->text + si->textlen);
	if (r <= 0) return;

	if (!conn->from) set_connection_state(conn, S_GETH);
	si->textlen += r;
}

static void
smb_got_data(struct connection *conn)
{
	struct smb_connection_info *si = conn->info;
	unsigned char buffer[READ_SIZE];
	ssize_t r;

	if (si->list_type != SMB_LIST_NONE) {
		smb_read_text(conn, conn->data_socket->fd);
		return;
	}

	r = smb_read_data(conn, conn->data_socket->fd, buffer);
	if (r <= 0) return;

	set_connection_state(conn, S_TRANS);

	if (smb_get_cache(conn)) return;

	conn->received += r;
	if (add_fragment(conn->cached, conn->from, buffer, r) == 1)
		conn->tries = 0;
	conn->from += r;
}

#undef READ_SIZE

static void
smb_got_text(struct connection *conn)
{
	smb_read_text(conn, conn->socket->fd);
}

 /* Search for @str1 followed by @str2 in @line.
  * It returns NULL if not found, or pointer to start
  * of @str2 in @line if found.  */
static unsigned char *
find_strs(unsigned char *line, unsigned char *str1,
	  unsigned char *str2)
{
	unsigned char *p = strstr(line, str1);

	if (!p) return NULL;

	p = strstr(p + strlen(str1), str2);
	return p;
}

static void
parse_smbclient_output(struct uri *uri, struct smb_connection_info *si,
		       struct string *page)
{
	unsigned char *line_start, *line_end;
	enum {
		SMB_TYPE_NONE,
		SMB_TYPE_SHARE,
		SMB_TYPE_SERVER,
		SMB_TYPE_WORKGROUP
	} type = SMB_TYPE_NONE;
	size_t pos = 0;
	int stop;

	assert(uri && si && page);
	if_assert_failed return;

	add_to_string(page, "<html><head><title>/");
	add_bytes_to_string(page, uri->data, uri->datalen);
	add_to_string(page, "</title></head><body><pre>");

	line_start = si->text;
	stop = !si->textlen;	/* Nothing to parse. */
	while (!stop && (line_end = strchr(line_start, ASCII_LF))) {
		unsigned char *line = line_start;
		size_t line_len;
		size_t start_offset = 0;

		/* Handle \r\n case. Normally, do not occur on *nix. */
		if (line_end > line_start && line_end[-1] == ASCII_CR)
			 line_end--, start_offset++;

		line_len = line_end - line_start;

		/* Here we modify si->text content, this should not be
		 * a problem as it is only used here. This prevents
		 * allocation of memory for the line. */
		*line_end = '\0';

		/* And I got bored here with cleaning it up. --pasky */

		if (si->list_type == SMB_LIST_SHARES) {
			unsigned char *ll, *lll, *found;

			if (!*line) type = SMB_TYPE_NONE;

			found = find_strs(line, "Sharename", "Type");
			if (found) {
				pos = found - line;
				type = SMB_TYPE_SHARE;
				goto print_as_is;
			}

			found = find_strs(line, "Server", "Comment");
			if (found) {
				type = SMB_TYPE_SERVER;
				goto print_as_is;
			}

			found = find_strs(line, "Workgroup", "Master");
			if (found) {
				pos = found - line;
				type = SMB_TYPE_WORKGROUP;
				goto print_as_is;
			}

			if (type == SMB_TYPE_NONE)
				goto print_as_is;

			for (ll = line; *ll; ll++)
				if (!isspace(*ll) && *ll != '-')
					goto print_next;

			goto print_as_is;

print_next:
			for (ll = line; *ll; ll++)
				if (!isspace(*ll))
					break;

			for (lll = ll; *lll; lll++)
				if (isspace(*lll))
					break;

			switch (type) {
			case SMB_TYPE_SHARE:
			{
				unsigned char *llll;

				if (!strstr(lll, "Disk"))
					goto print_as_is;

				if (pos && pos < line_len
				    && isspace(*(llll = line + pos - 1))
				    && llll > ll) {
					while (llll > ll && isspace(*llll))
						llll--;
					if (!isspace(*llll))
						lll = llll + 1;
				}

				add_bytes_to_string(page, line, ll - line);
				add_to_string(page, "<a href=\"");
				add_bytes_to_string(page, ll, lll - ll);
				add_to_string(page, "/\">");
				add_bytes_to_string(page, ll, lll - ll);
				add_to_string(page, "</a>");
				add_to_string(page, lll);
				break;
			}

			case SMB_TYPE_WORKGROUP:
				if (pos < line_len && pos
				    && isspace(line[pos - 1])
				    && !isspace(line[pos])) {
					ll = line + pos;
				} else {
					for (ll = lll; *ll; ll++)
						if (!isspace(*ll))
							break;
				}
				for (lll = ll; *lll; lll++)
					if (isspace(*lll))
						break;
				/* Fall-through */

			case SMB_TYPE_SERVER:
				add_bytes_to_string(page, line, ll - line);
				add_to_string(page, "<a href=\"smb://");
				add_bytes_to_string(page, ll, lll - ll);
				add_to_string(page, "/\">");
				add_bytes_to_string(page, ll, lll - ll);
				add_to_string(page, "</a>");
				add_to_string(page, lll);
				break;

			case SMB_TYPE_NONE:
				goto print_as_is;
			}

		} else if (si->list_type == SMB_LIST_DIR) {
			if (strstr(line, "NT_STATUS")) {
				/* Error, stop after message. */
				stop = 1;
				goto print_as_is;
			}

			if (line_end - line_start >= 5
			    && line_start[0] == ' '
			    && line_start[1] == ' '
			    && line_start[2] != ' ') {
				int dir = 0;
				int may_be_dir = 0;
				unsigned char *p = line_end;
				unsigned char *url = line_start + 2;

				/* smbclient list parser
				 * The boring thing is that output is
				 * ambiguous in many ways:
				 * filenames with more than one space,
				 * etc...
				 * This bloated code tries to do a not
				 * so bad job. --Zas */

/* directory                      D        0  Fri May  7 11:23:18 2004 */
/* filename                             2444  Thu Feb 19 15:52:46 2004 */

				/* Skip end of line */
				while (p > url && !isdigit(*p)) p--;
				if (p == url) goto print_as_is;

				/* FIXME: Use parse_date()? */
				/* year */
				while (p > url && isdigit(*p)) p--;
				if (p == url || !isspace(*p)) goto print_as_is;
				while (p > url && isspace(*p)) p--;

				/* seconds */
				while (p > url && isdigit(*p)) p--;
				if (p == url || *p != ':') goto print_as_is;
				p--;

				/* minutes */
				while (p > url && isdigit(*p)) p--;
				if (p == url || *p != ':') goto print_as_is;
				p--;

				/* hours */
				while (p > url && isdigit(*p)) p--;
				if (p == url || !isspace(*p)) goto print_as_is;
				p--;

				/* day as number */
				while (p > url && isdigit(*p)) p--;
				while (p > url && isspace(*p)) p--;
				if (p == url) goto print_as_is;

				/* month */
				while (p > url && !isspace(*p)) p--;
				if (p == url || !isspace(*p)) goto print_as_is;
				p--;

				/* day name */
				while (p > url && !isspace(*p)) p--;
				if (p == url || !isspace(*p)) goto print_as_is;
				while (p > url && isspace(*p)) p--;

				/* file size */
				if (p == url || !isdigit(*p)) goto print_as_is;

				if (*p == '0' && isspace(*(p - 1))) may_be_dir = 1;

				while (p > url && isdigit(*p)) p--;
				if (p == url) goto print_as_is;

				/* Magic to determine if we have a
				 * filename or a dirname. Thanks to
				 * smbclient ambiguous output. */
				{
					unsigned char *pp = p;

					while (pp > url && isspace(*pp)) pp--;

					if (p - pp <= 8) {
						while (pp > url
						       && (*pp == 'D'
							  || *pp == 'H'
							  || *pp == 'A'
							  || *pp == 'S'
						          || *pp == 'R'
							  || *pp == 'V')) {
						        if (*pp == 'D' && may_be_dir)
								dir = 1;
							pp--;
						}
					}
					while (pp > url && isspace(*pp)) pp--;
					p = pp;
				}

				/* Don't display '.' directory */
				if (p == url && *url == '.') goto ignored;
				p++;

				add_to_string(page, "  <a href=\"");
				add_bytes_to_string(page, url, p - url);
				if (dir) add_char_to_string(page, '/');
				add_to_string(page, "\">");
				add_bytes_to_string(page, url, p - url);
				add_to_string(page, "</a>");
				add_bytes_to_string(page, p, line_end - p);

			} else {
				goto print_as_is;
			}

		} else {
print_as_is:
			add_bytes_to_string(page, line_start, line_len);
		}

		add_char_to_string(page, ASCII_LF);
ignored:
		line_start = line_end + start_offset + 1;
	}

	add_to_string(page, "</pre></body></html>");
}

static void
end_smb_connection(struct connection *conn)
{
	struct smb_connection_info *si = conn->info;
	struct uri *uri;
	enum connection_state state = S_OK;

	if (smb_get_cache(conn)) return;

	if (conn->from) {
		normalize_cache_entry(conn->cached, conn->from);
		goto bye;
	}

	/* Ensure termination by LF + NUL chars, memory for this
	 * was reserved by smb_read_text(). */
	if (si->textlen && si->text[si->textlen - 1] != ASCII_LF)
		si->text[si->textlen++] = ASCII_LF;
	si->text[si->textlen] = '\0';

	uri = conn->uri;
	if (uri->datalen
	    && uri->data[uri->datalen - 1] != '/'
	    && uri->data[uri->datalen - 1] != '\\'
	    && (strstr(si->text, "NT_STATUS_FILE_IS_A_DIRECTORY")
		|| strstr(si->text, "NT_STATUS_ACCESS_DENIED")
		|| strstr(si->text, "ERRbadfile"))) {
		redirect_cache(conn->cached, "/", 1, 0);

	} else {
		struct string page;

		if (!init_string(&page)) {
			state = S_OUT_OF_MEM;
			goto bye;
		}

		parse_smbclient_output(uri, si, &page);

		add_fragment(conn->cached, 0, page.source, page.length);
		conn->from += page.length;
		normalize_cache_entry(conn->cached, page.length);
		done_string(&page);

		mem_free_set(&conn->cached->content_type, stracpy("text/html"));
	}

bye:
	close_socket(conn->socket);
	close_socket(conn->data_socket);
	abort_connection(conn, state);
}


/* Close all non-terminal file descriptors. */
static void
close_all_non_term_fd(void)
{
	int n;
	int max = 1024;
#ifdef RLIMIT_NOFILE
	struct rlimit lim;

	if (!getrlimit(RLIMIT_NOFILE, &lim))
		max = lim.rlim_max;
#endif
	for (n = 3; n < max; n++)
		close(n);
}

void
smb_protocol_handler(struct connection *conn)
{
	int out_pipe[2] = { -1, -1 };
	int err_pipe[2] = { -1, -1 };
	unsigned char *share, *dir;
	unsigned char *p;
	pid_t cpid;
	int dirlen;
	struct smb_connection_info *si;
	struct uri *uri;

	si = mem_calloc(1, sizeof(*si) + 2);
	if (!si) {
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}
	conn->info = si;

	uri = conn->uri;
	p = strchr(uri->data, '/');
	if (p && p - uri->data < uri->datalen) {
		share = memacpy(uri->data, p - uri->data);
		dir = p + 1;
		/* FIXME: ensure @dir do not contain dangerous chars. --Zas */

	} else if (uri->datalen) {
		if (smb_get_cache(conn)) return;

		redirect_cache(conn->cached, "/", 1, 0);
		abort_connection(conn, S_OK);
		return;

	} else {
		share = stracpy("");
		dir = "";
	}

	if (!share) {
		abort_connection(conn, S_OUT_OF_MEM);
		return;
	}

	dirlen = strlen(dir);
	if (!*share) {
		si->list_type = SMB_LIST_SHARES;
	} else if (!dirlen || dir[dirlen - 1] == '/'
		   || dir[dirlen - 1] == '\\') {
		si->list_type = SMB_LIST_DIR;
	}

	if (c_pipe(out_pipe) || c_pipe(err_pipe)) {
		int s_errno = errno;

		if (out_pipe[0] >= 0) close(out_pipe[0]);
		if (out_pipe[1] >= 0) close(out_pipe[1]);
		mem_free(share);
		abort_connection(conn, -s_errno);
		return;
	}

	conn->from = 0;

	cpid = fork();
	if (cpid == -1) {
		int s_errno = errno;

		close(out_pipe[0]);
		close(out_pipe[1]);
		close(err_pipe[0]);
		close(err_pipe[1]);
		mem_free(share);
		retry_connection(conn, -s_errno);
		return;
	}

	if (!cpid) {
#define SMBCLIENT "smbclient"
#define MAX_SMBCLIENT_ARGS 32
		int n = 0;
		unsigned char *v[MAX_SMBCLIENT_ARGS];
		unsigned char *optstr;

		close(1);
		dup2(out_pipe[1], 1);
		close(2);
		dup2(err_pipe[1], 2);
		close(0);
		dup2(open("/dev/null", O_RDONLY), 0);

		close_all_non_term_fd();
		close(out_pipe[0]);
		close(err_pipe[0]);

		/* Usage: smbclient service <password> [options] */
		v[n++] = SMBCLIENT;

		/* FIXME: handle alloc failures. */
		/* At this point, we are the child process.
		 * Maybe we just don't care if the child kills itself
		 * dereferencing a NULL pointer... -- Miciah */
		/* Leaving random core files after itself is not what a nice
		 * program does. Also, the user might also want to know, why
		 * the hell does he see nothing on the screen. --pasky */

		if (*share) {
			/* Construct service path. */
			asprintf((char **) &v[n++], "//%.*s/%s",
				 uri->hostlen, uri->host, share);

			/* Add password if any. */
			if (uri->passwordlen && !uri->userlen) {
				v[n++] = memacpy(uri->password, uri->passwordlen);
			}
		} else {
			/* Get a list of shares available on a host. */
			v[n++] = "-L";
			v[n++] = memacpy(uri->host, uri->hostlen);
		}

		v[n++] = "-N";		/* Don't ask for a password. */
		v[n++] = "-E";		/* Write messages to stderr instead of stdout. */
		v[n++] = "-d 0";	/* Disable debug mode. */

		if (uri->portlen) {
			/* Connect to the specified port. */
			v[n++] = "-p";
			v[n++] = memacpy(uri->port, uri->portlen);
		}

		if (uri->userlen) {
			/* Set the network username. */
			v[n++] = "-U";
			if (!uri->passwordlen) {
				/* No password. */
				v[n++] = memacpy(uri->user, uri->userlen);
			} else {
				/* With password. */
				asprintf((char **) &v[n++], "%.*s%%%.*s",
					 uri->userlen, uri->user,
					 uri->passwordlen, uri->password);
			}
		}

		if (*share) {
			/* FIXME: use si->list_type here ?? --Zas */
			if (!dirlen || dir[dirlen - 1] == '/' || dir[dirlen - 1] == '\\') {
				if (dirlen) {
					/* Initial directory. */
					v[n++] = "-D";
					v[n++] = dir;
				}

				v[n++] = "-c"; /* Execute semicolon separated commands. */
				v[n++] = "ls"; /* List files. */

			} else {
				/* Copy remote file to stdout. */
				unsigned char *s = straconcat("get \"", dir, "\" -", NULL);
				unsigned char *ss = s;

				v[n++] = "-c"; /* Execute semicolon separated commands. */
				while ((ss = strchr(ss, '/'))) *ss = '\\'; /* Escape '/' */
				v[n++] = s;
			}
		}

		/* Optionally add SMB credentials file. */
		optstr = get_opt_str("protocol.smb.credentials");
		if (optstr[0]) {
			v[n++] = "-A";
			v[n++] = optstr;
		}

		v[n++] = NULL;	/* End of arguments list. */
		assert(n < MAX_SMBCLIENT_ARGS);

		execvp(SMBCLIENT, (char **) v);

		/* FIXME: this message will never be displayed, since execvp()
		 * failed. */
		fprintf(stderr, SMBCLIENT " not found in $PATH");
		_exit(1);
#undef MAX_SMBCLIENT_ARGS
#undef SMBCLIENT
	}

	mem_free(share);

	conn->data_socket->fd = out_pipe[0];
	conn->socket->fd = err_pipe[0];

	close(out_pipe[1]);
	close(err_pipe[1]);

	set_handlers(out_pipe[0], (select_handler_T) smb_got_data, NULL, NULL, conn);
	set_handlers(err_pipe[0], (select_handler_T) smb_got_text, NULL, NULL, conn);
	set_connection_state(conn, S_CONN);
}
