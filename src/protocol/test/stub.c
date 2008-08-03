#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/msgbox.h"
#include "main/module.h"
#include "protocol/user.h"
#include "session/session.h"
#include "util/test.h"

#define STUB_MODULE(name)				\
	struct module name = struct_module(		\
		/* name: */		"Stub " #name,	\
		/* options: */		NULL,		\
		/* hooks: */		NULL,		\
		/* submodules: */	NULL,		\
		/* data: */		NULL,		\
		/* init: */		NULL,		\
		/* done: */		NULL		\
	)
STUB_MODULE(auth_module);
STUB_MODULE(bittorrent_protocol_module);
STUB_MODULE(cgi_protocol_module);
STUB_MODULE(file_protocol_module);
STUB_MODULE(finger_protocol_module);
STUB_MODULE(fsp_protocol_module);
STUB_MODULE(ftp_protocol_module);
STUB_MODULE(gopher_protocol_module);
STUB_MODULE(http_protocol_module);
STUB_MODULE(nntp_protocol_module);
STUB_MODULE(smb_protocol_module);
STUB_MODULE(uri_rewrite_module);
STUB_MODULE(user_protocol_module);

static void
stub_called(const unsigned char *fun)
{
	die("FAIL: stub %s\n", fun);
}

#define STUB_PROTOCOL_HANDLER(name)		\
	void					\
	name(struct connection *conn)		\
	{					\
		stub_called(#name);		\
	}					\
	protocol_handler_T name /* consume semicolon */
#define STUB_PROTOCOL_EXTERNAL_HANDLER(name)		\
	void						\
	name(struct session *ses, struct uri *uri)	\
	{						\
		stub_called(#name);			\
	}						\
	protocol_external_handler_T name /* consume semicolon */
STUB_PROTOCOL_HANDLER(about_protocol_handler);
STUB_PROTOCOL_HANDLER(bittorrent_protocol_handler);
STUB_PROTOCOL_HANDLER(data_protocol_handler);
STUB_PROTOCOL_EXTERNAL_HANDLER(ecmascript_protocol_handler);
STUB_PROTOCOL_HANDLER(file_protocol_handler);
STUB_PROTOCOL_HANDLER(finger_protocol_handler);
STUB_PROTOCOL_HANDLER(fsp_protocol_handler);
STUB_PROTOCOL_HANDLER(ftp_protocol_handler);
STUB_PROTOCOL_HANDLER(gopher_protocol_handler);
STUB_PROTOCOL_HANDLER(http_protocol_handler);
STUB_PROTOCOL_HANDLER(news_protocol_handler);
STUB_PROTOCOL_HANDLER(nntp_protocol_handler);
STUB_PROTOCOL_HANDLER(proxy_protocol_handler);
STUB_PROTOCOL_HANDLER(smb_protocol_handler);
STUB_PROTOCOL_EXTERNAL_HANDLER(user_protocol_handler);

/* declared in "protocol/user.h" */
unsigned char *
get_user_program(struct terminal *term, unsigned char *progid, int progidlen)
{
	stub_called("get_user_program");
	return NULL;
}

/* declared in "session/session.h" */
void
print_error_dialog(struct session *ses, struct connection_state state,
		   struct uri *uri, enum connection_priority priority)
{
	stub_called("print_error_dialog");
}

/* declared in "bfu/msgbox.h" */
unsigned char *
msg_text(struct terminal *term, unsigned char *format, ...)
{
	stub_called("msg_text");
	return NULL;
}

/* declared in "bfu/msgbox.h" */
struct dialog_data *
msg_box(struct terminal *term, struct memory_list *mem_list,
	enum msgbox_flags flags, unsigned char *title, enum format_align align,
	unsigned char *text, void *udata, int buttons, ...)
{
	/* mem_list should be freed here but because this is just a
	 * test program it won't matter.  */
	stub_called("msg_box");
	return NULL;
}

