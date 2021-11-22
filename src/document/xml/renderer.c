/* Plain text document renderer */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bookmarks/bookmarks.h"
#include "cache/cache.h"
#include "config/options.h"
#include "document/docdata.h"
#include "document/document.h"
#include "document/format.h"
#include "document/options.h"
#include "document/plain/renderer.h"
#include "document/xml/renderer.h"
#include "document/renderer.h"
#ifdef CONFIG_ECMASCRIPT
#include "ecmascript/ecmascript.h"
#endif
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

#include <libxml++/libxml++.h>
#include <string>

void
render_source_document_cxx(struct cache_entry *cached, struct document *document,
		      struct string *buffer)
{
	char *head = empty_string_or_(cached->head);

	(void)get_convert_table(head, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

	if (!document->dom) {
		document->dom = document_parse(document);
	}
	if (document->dom) {
		xmlpp::Document *docu = document->dom;
		xmlpp::ustring text = docu->write_to_string_formatted(get_cp_mime_name(document->cp));
		struct string tt;
		init_string(&tt);
		add_bytes_to_string(&tt, text.c_str(), text.size());
		render_plain_document(cached, document, &tt);
		done_string(&tt);
	}
}
