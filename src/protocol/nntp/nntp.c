/* Network news transport protocol implementation (RFC 977 and 2980) */


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "main/module.h"
#include "protocol/nntp/nntp.h"

/* The official color for this planet is green,
 * which grows in pockets of them people willing to scheme. --delasoul */

#define NNTP_HEADER_ENTRIES "Subject,From,Date,Message-ID,Newsgroups"

/* Module and option stuff: */

enum nntp_protocol_option {
	NNTP_PROTOCOL_TREE,

	NNTP_PROTOCOL_SERVER,
	NNTP_PROTOCOL_HEADER_ENTRIES,

	NNTP_PROTOCOL_OPTIONS,
};

static union option_info nntp_protocol_options[] = {
	INIT_OPT_TREE("protocol", N_("NNTP"),
		"nntp", 0,
		N_("NNTP and news specific options.")),

	INIT_OPT_STRING("protocol.nntp", N_("Default news server"),
		"server", 0, "",
		N_("Used when resolving news: URIs. "
		"If set to the empty string the value of the NNTPSERVER "
		"environment variable will be used.")),

	INIT_OPT_STRING("protocol.nntp", N_("Message header entries"),
		"header_entries", 0, NNTP_HEADER_ENTRIES,
		N_("Comma separated list of which entries in the article "
		"header to show. E.g. 'Subject' and 'From'. "
		"All header entries can be read in the header info dialog.")),

	NULL_OPTION_INFO,
};

#define get_opt_nntp(which)	nntp_protocol_options[(which)].option

unsigned char *
get_nntp_server(void)
{
	return get_opt_nntp(NNTP_PROTOCOL_SERVER).value.string;
}

unsigned char *
get_nntp_header_entries(void)
{
	return get_opt_nntp(NNTP_PROTOCOL_HEADER_ENTRIES).value.string;
}

struct module nntp_protocol_module = struct_module(
	/* name: */		N_("NNTP"),
	/* options: */		nntp_protocol_options,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
