#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/msgbox.h"		/* msg_text, msg_box */
#include "main/module.h"
#include "protocol/test/harness.h"
#include "protocol/user.h"	/* get_user_program */
#include "session/session.h"	/* print_error_dialog */

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
STUB_MODULE(cgi_protocol_module);
STUB_MODULE(file_protocol_module);
STUB_MODULE(fsp_protocol_module);
STUB_MODULE(ftp_protocol_module);
STUB_MODULE(http_protocol_module);
STUB_MODULE(smb_protocol_module);
STUB_MODULE(uri_rewrite_module);
STUB_MODULE(user_protocol_module);

static void
stub_called(const unsigned char *fun)
{
	fprintf(stderr, "FAIL: stub %s\n", fun);
	test_failed();
}

#define STUB_PROTOCOL_HANDLER(name)			\
	void						\
	name(struct session *ses, struct uri *uri)	\
	{						\
		stub_called(#name);			\
	}
STUB_PROTOCOL_HANDLER(about_protocol_handler)
STUB_PROTOCOL_HANDLER(data_protocol_handler)
STUB_PROTOCOL_HANDLER(ecmascript_protocol_handler)
STUB_PROTOCOL_HANDLER(file_protocol_handler)
STUB_PROTOCOL_HANDLER(fsp_protocol_handler)
STUB_PROTOCOL_HANDLER(ftp_protocol_handler)
STUB_PROTOCOL_HANDLER(http_protocol_handler)
STUB_PROTOCOL_HANDLER(proxy_protocol_handler)
STUB_PROTOCOL_HANDLER(smb_protocol_handler)
STUB_PROTOCOL_HANDLER(user_protocol_handler)

unsigned char *
get_user_program(struct terminal *term, unsigned char *progid, int progidlen)
{
	stub_called("get_user_program");
	return NULL;
}

void
print_error_dialog(struct session *ses, enum connection_state state,
		   struct uri *uri, enum connection_priority priority)
{
	stub_called("print_error_dialog");
}

unsigned char *
msg_text(struct terminal *term, unsigned char *format, ...)
{
	stub_called("msg_text");
	return NULL;
}

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

