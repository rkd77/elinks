/* Internal "mailto", "telnet", "tn3270" and misc. protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bfu/dialog.h"
#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "osdep/osdep.h"
#include "protocol/uri.h"
#include "protocol/user.h"
#include "session/download.h"
#include "session/session.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/string.h"


static union option_info user_protocol_options[] = {
	INIT_OPT_TREE("protocol", N_("User protocols"),
		"user", OPT_AUTOCREATE,
		N_("User protocols. Options in this tree specify external "
		"handlers for the appropriate protocols. Ie. "
		"protocol.user.mailto.unix.")),

	/* FIXME: Poorly designed options structure. Ought to be able to specify
	 * need_slashes, free_form and similar options as well :-(. --pasky */

	/* Basically, it looks like protocol.user.mailto.win32 = "blah" */

	INIT_OPT_TREE("protocol.user", NULL,
		"_template_", OPT_AUTOCREATE,
		N_("Handler (external program) for this protocol. Name the "
		"options in this tree after your system (ie. unix, "
		"unix-xwin).")),

	INIT_OPT_STRING("protocol.user._template_", NULL,
		"_template_", 0, "",
		N_("Handler (external program) for this protocol and system.\n"
		"%f in the string means file name to include form data from\n"
		"%h in the string means hostname (or email address)\n"
		"%p in the string means port\n"
		"%d in the string means path (everything after the port)\n"
		"%s in the string means subject (?subject=<this>)\n"
		"%u in the string means the whole URL")),

#define INIT_OPT_USER_PROTOCOL(scheme, system, cmd) \
	INIT_OPT_STRING("protocol.user." scheme, NULL, system, 0, cmd, NULL)

#ifndef CONFIG_GOPHER
	INIT_OPT_USER_PROTOCOL("gopher", "unix",	DEFAULT_AC_OPT_GOPHER),
	INIT_OPT_USER_PROTOCOL("gopher", "unix-xwin",	DEFAULT_AC_OPT_GOPHER),
#endif
	INIT_OPT_USER_PROTOCOL("irc",	 "unix",	DEFAULT_AC_OPT_IRC),
	INIT_OPT_USER_PROTOCOL("irc",	 "unix-xwin",	DEFAULT_AC_OPT_IRC),
	INIT_OPT_USER_PROTOCOL("mailto", "unix",	DEFAULT_AC_OPT_MAILTO),
	INIT_OPT_USER_PROTOCOL("mailto", "unix-xwin",	DEFAULT_AC_OPT_MAILTO),
#ifndef CONFIG_NNTP
	INIT_OPT_USER_PROTOCOL("news",	 "unix",	DEFAULT_AC_OPT_NEWS),
	INIT_OPT_USER_PROTOCOL("news",	 "unix-xwin",	DEFAULT_AC_OPT_NEWS),
#endif
	INIT_OPT_USER_PROTOCOL("telnet", "unix",	DEFAULT_AC_OPT_TELNET),
	INIT_OPT_USER_PROTOCOL("telnet", "unix-xwin",	DEFAULT_AC_OPT_TELNET),
	INIT_OPT_USER_PROTOCOL("tn3270", "unix",	DEFAULT_AC_OPT_TN3270),
	INIT_OPT_USER_PROTOCOL("tn3270", "unix-xwin",	DEFAULT_AC_OPT_TN3270),

	NULL_OPTION_INFO,
};

struct module user_protocol_module = struct_module(
	/* name: */		N_("User protocols"),
	/* options: */		user_protocol_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);


unsigned char *
get_user_program(struct terminal *term, unsigned char *progid, int progidlen)
{
	struct option *opt;
	int xwin = term ? term->environment & ENV_XWIN : 0;
	struct string name;

	if (!init_string(&name)) return NULL;

	add_to_string(&name, "protocol.user.");

	/* Now add lowercased progid part. Delicious. */
	add_bytes_to_string(&name, progid, progidlen);
	convert_to_lowercase_locale_indep(&name.source[sizeof("protocol.user.") - 1], progidlen);

	add_char_to_string(&name, '.');
	add_to_string(&name, get_system_str(xwin));

	opt = get_opt_rec_real(config_options, name.source);

	done_string(&name);
	return (unsigned char *) (opt ? opt->value.string : NULL);
}


static unsigned char *
subst_cmd(unsigned char *cmd, struct uri *uri, unsigned char *subj,
	  unsigned char *formfile)
{
	struct string string;

	if (!init_string(&string)) return NULL;

	while (*cmd) {
		int p;

		for (p = 0; cmd[p] && cmd[p] != '%'; p++);

		add_bytes_to_string(&string, cmd, p);
		cmd += p;

		if (*cmd != '%') break;

		cmd++;
		/* TODO: Decode URI fragments before adding them. --jonas */
		switch (*cmd) {
			case 'u':
			{
				unsigned char *url = struri(uri);
				int length = get_real_uri_length(uri);

				add_shell_safe_to_string(&string, url, length);
				break;
			}
			case 'h':
				/* TODO	For some user protocols it would be
				 *	better if substitution of each uri
				 *	field was completely configurable. Now
				 *	@host contains both the uri username
				 *	field, (password field) and hostname
				 *	field because it is useful for mailto
				 *	protocol handling. */
				/* It would break a lot of configurations so I
				 * don't know. --jonas */
				if (uri->userlen && uri->hostlen) {
					int hostlen = uri->host + uri->hostlen - uri->user;

					add_shell_safe_to_string(&string, uri->user,
								 hostlen);
				} else if (uri->host) {
					add_shell_safe_to_string(&string, uri->host,
								 uri->hostlen);
				}
				break;
			case 'p':
				if (uri->portlen)
					add_shell_safe_to_string(&string, uri->port,
								 uri->portlen);
				break;
			case 'd':
				if (uri->datalen)
					add_shell_safe_to_string(&string, uri->data,
								 uri->datalen);
				break;
			case 's':
				if (subj)
					add_shell_safe_to_string(&string, subj,
								 strlen(subj));
				break;
			case 'f':
				if (formfile)
					add_to_string(&string, formfile);
				break;
			default:
				add_bytes_to_string(&string, cmd - 1, 2);
				break;
		}
		if (*cmd) cmd++;
	}

	return string.source;
}

/* Stay silent about complete RFC 2368 support or do it yourself! ;-).
 * --pasky */
static unsigned char *
get_subject_from_query(unsigned char *query)
{
	unsigned char *subject;

	if (strncmp(query, "subject=", 8)) {
		subject = strstr(query, "&subject=");
		if (!subject) return NULL;
		subject += 9;
	} else {
		subject = query + 8;
	}

	/* Return subject until next '&'-value or end of string */
	return memacpy(subject, strcspn(subject, "&"));
}

static unsigned char *
save_form_data_to_file(struct uri *uri)
{
	unsigned char *filename = get_tempdir_filename("elinks-XXXXXX");
	int fd;
	FILE *fp;
	size_t nmemb, len;
	unsigned char *formdata;

	if (!filename) return NULL;

	fd = safe_mkstemp(filename);
	if (fd < 0) {
		mem_free(filename);
		return NULL;
	}

	if (!uri->post) return filename;

	/* Jump the content type */
	formdata = strchr(uri->post, '\n');
	formdata = formdata ? formdata + 1 : uri->post;
	len = strlen(formdata);
	if (len == 0) return filename;

	fp = fdopen(fd, "w");
	if (!fp) {

error:
		unlink(filename);
		mem_free(filename);
		close(fd);
		return NULL;
	}

	nmemb = fwrite(formdata, len, 1, fp);
	if (nmemb != 1) {
		fclose(fp);
		goto error;
	}

	if (fclose(fp) != 0)
		goto error;

	return filename;
}

void
user_protocol_handler(struct session *ses, struct uri *uri)
{
	unsigned char *subj = NULL, *prog;
	unsigned char *filename;

	prog = get_user_program(ses->tab->term, struri(uri), uri->protocollen);
	if (!prog || !*prog) {
		unsigned char *protocol = memacpy(struri(uri), uri->protocollen);

		/* Shouldn't ever happen, but be paranoid. */
		/* Happens when you're in X11 and you've no handler for it. */
		info_box(ses->tab->term, MSGBOX_FREE_TEXT,
			 N_("No program"), ALIGN_CENTER,
			 msg_text(ses->tab->term,
				 N_("No program specified for protocol %s."),
				 empty_string_or_(protocol)));

		mem_free_if(protocol);
		return;
	}

	if (uri->data && uri->datalen) {
		/* Some mailto specific stuff follows... */
		unsigned char *query = get_uri_string(uri, URI_QUERY);

		if (query) {
			subj = get_subject_from_query(query);
			mem_free(query);
			if (subj) decode_uri(subj);
		}
	}

	filename = save_form_data_to_file(uri);

	prog = subst_cmd(prog, uri, subj, filename);
	mem_free_if(subj);
	if (prog) {
		unsigned char *delete = empty_string_or_(filename);

		exec_on_terminal(ses->tab->term, prog, delete, TERM_EXEC_FG);
		mem_free(prog);

	} else if (filename) {
		unlink(filename);
	}

	mem_free_if(filename);
}
