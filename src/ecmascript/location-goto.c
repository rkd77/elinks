/** Let web scripts navigate to a different location.
 * Used for both SpiderMonkey and SEE.
 * @file */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "document/document.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h" /* declares location_goto() defined here */
#include "main/select.h"
#include "protocol/uri.h"
#include "session/task.h"
#include "util/conv.h"
#include "util/error.h"
#include "viewer/text/vs.h"

static void delayed_goto(void *);

struct delayed_goto {
	/* It might look more convenient to pass doc_view around but it could
	 * disappear during wild dances inside of frames or so. */
	struct view_state *vs;
	struct uri *uri;
};

static void
delayed_goto(void *data)
{
	struct delayed_goto *deg = data;

	assert(deg);
	if (deg->vs->doc_view
	    && deg->vs->doc_view == deg->vs->doc_view->session->doc_view) {
		goto_uri_frame(deg->vs->doc_view->session, deg->uri,
		               deg->vs->doc_view->name,
			       CACHE_MODE_NORMAL);
	}
	done_uri(deg->uri);
	mem_free(deg);
}

void
location_goto(struct document_view *doc_view, unsigned char *url)
{
	unsigned char *new_abs_url;
	struct uri *new_uri;
	struct delayed_goto *deg;

	/* Workaround for bug 611. Does not crash, but may lead to infinite loop.*/
	if (!doc_view) return;
	new_abs_url = join_urls(doc_view->document->uri,
	                        trim_chars(url, ' ', 0));
	if (!new_abs_url)
		return;
	new_uri = get_uri(new_abs_url, 0);
	mem_free(new_abs_url);
	if (!new_uri)
		return;
	deg = mem_calloc(1, sizeof(*deg));
	if (!deg) {
		done_uri(new_uri);
		return;
	}
	assert(doc_view->vs);
	deg->vs = doc_view->vs;
	deg->uri = new_uri;
	/* It does not seem to be very safe inside of frames to
	 * call goto_uri() right away. */
	register_bottom_half(delayed_goto, deg);
}
