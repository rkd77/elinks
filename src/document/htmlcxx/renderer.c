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
#include "document/htmlcxx/renderer.h"
#include "document/plain/renderer.h"
#include "document/renderer.h"
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

#include <htmlcxx/html/ParserDom.h>
using namespace htmlcxx;

struct source_renderer {
	struct string tmp_buffer;
	struct string *source;
	char *enc;
};

static void
walk_tree(struct string *buf, tree<HTML::Node> const &dom)
{
	tree<HTML::Node>::iterator it = dom.begin();
	add_to_string(buf, it->text().c_str());

	for (tree<HTML::Node>::sibling_iterator childIt = dom.begin(it); childIt != dom.end(it); ++childIt)
	{
		walk_tree(buf, childIt);
	}
	add_to_string(buf, it->closingText().c_str());
}

static int
htmlcxx_main(struct source_renderer *renderer)
{
	std::string html = renderer->source->source;
	HTML::ParserDom parser;
	tree<HTML::Node> dom = parser.parseTree(html);
	walk_tree(&renderer->tmp_buffer, dom);

	return 0;
}

void
render_source_document_cxx(struct cache_entry *cached, struct document *document,
		      struct string *buffer)
{
	struct source_renderer renderer;
	char *head = empty_string_or_(cached->head);

	(void)get_convert_table(head, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

	init_string(&renderer.tmp_buffer);
	renderer.source = buffer;
	renderer.enc = get_cp_mime_name(document->cp);
	htmlcxx_main(&renderer);
	render_plain_document(cached, document, &renderer.tmp_buffer);
	done_string(&renderer.tmp_buffer);
}
