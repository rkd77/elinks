/* Gemini query dialog stuff */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/inpfield.h"
#include "intl/libintl.h"
#include "main/object.h"
#include "protocol/gemini/dialog.h"
#include "protocol/gemini/gemini.h"
#include "protocol/uri.h"
#include "session/location.h"
#include "session/session.h"
#include "session/task.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"

static void
query_ok(void *d, const char *data)
{
	struct gemini_error_info *info = (struct gemini_error_info *)d;

	char *url = get_uri_string(info->uri, URI_PROTOCOL | URI_HOST | URI_PORT | URI_DATA);
	char *url2 = straconcat(url, "?", data, NULL);

	mem_free_if(url);

	if (!url2) {
		return;
	}
	goto_url_with_hook(info->ses, url2);
	mem_free(url2);
	done_uri(info->uri);
	mem_free(info->prompt);
	mem_free(info);
}

static void
query_cancel(void *d)
{
	struct gemini_error_info *info = (struct gemini_error_info *)d;

	done_uri(info->uri);
	mem_free(info->prompt);
	mem_free(info);
}

void
do_gemini_query_dialog(struct session *ses, void *data)
{
	struct gemini_error_info *info = (struct gemini_error_info *)data;
	info->ses = ses;

	input_dialog(ses->tab->term, NULL,
		     info->prompt, N_("Enter text"),
		     info, NULL,
		     MAX_STR_LEN, info->value, 0, 0, NULL,
		     (void (*)(void *, char *))query_ok,
		     query_cancel);
}
