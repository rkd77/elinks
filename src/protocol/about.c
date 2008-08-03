/* Pseudo about: protocol implementation */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "cache/cache.h"
#include "network/connection.h"
#include "protocol/about.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "util/string.h"


#ifndef CONFIG_SMALL
struct about_page {
	unsigned char *name;
	unsigned char *string;
};

static const struct about_page about_pages[] = {
	{ "bloat",

		"<html><body>"
		"<p>Bloat? What bloat?</p>"
		"</body></html>"

	},
	{ "links",

		"<html><body><pre>"
		"/*                 D~~)w  */\n"
		"/*                /    |  */\n"
		"/*             /'m_O   /  */\n"
		"/*           /'.\"//~~~~\\  */\n"
		"/*           `\\/`\\`--') , */\n"
		"/*             /  `~~~  | */\n"
		"/*            |         | */\n"
		"/*            |         , */\n"
		"/*            `_'p~~~~~/  */\n"
		"/*              .  ||_|   */\n"
		"/*          `  .  _|| |   */\n"
		"/*           `, ((____|   */\n"
		"</pre></body></html>"

	},
	{ "mozilla",

		"<html><body text=\"white\" bgcolor=\"red\">"
		"<p align=\"center\">And the <em>beste</em> shall meet "
		"a <em>being</em> and the being shall wear signs "
		"of EL and the signs shall have colour of enke. "
		"And the being shall be of <em>Good Nature</em>. "
		"From on high the beste hath looked down upon the being "
		"and the being was <em>smal</em> compared to it. "
		"Yet <em>faster</em> and <em>leaner</em> it hath been "
		"and it would come through doors closed to the beste. "
		"And there was truly great <em>respect</em> "
		"twix the beste and the <em>smal being</em> "
		"and they bothe have <em>served</em> to naciouns. "
		"Yet only the <em>true believers</em> "
		"would choose betwene them and the followers "
		"of <em>mammon</em> stayed <em>blinded</em> to bothe.</p>"
		"<p align=\"right\">from <em>The Book of Mozilla</em> "
		"(Apocryphon of ELeasar), 12:24</p>"
		"</body></html>"

	},
	{ "fear",

		"<html><body text=\"yellow\">"
		"<p>I must not fear. Fear is the mind-killer. "
		"Fear is the little-death that brings total obliteration. "
		"I will face my fear. "
		"I will permit it to pass over me and through me. "
		"And when it has gone past I will turn the inner eye "
		"to see its path. "
		"Where the fear has gone there will be nothing. "
		"Only I will remain.</p>"
		"</body></html>"

	},

	{ NULL, NULL }
};
#endif

void
about_protocol_handler(struct connection *conn)
{
	struct cache_entry *cached = get_cache_entry(conn->uri);

	/* Only do this the first time */
	if (cached && !cached->content_type) {
#ifndef CONFIG_SMALL
		{
			const struct about_page *page = about_pages;

			for (; page->name; page++) {
				int len;
				unsigned char *str;

				if (strcmp(conn->uri->data, page->name))
					continue;

				str = page->string;
				len = strlen(str);
				add_fragment(cached, 0, str, len);
				conn->from = len;
				break;
			}
		}
#endif

		/* Set content to known type */
		mem_free_set(&cached->content_type, stracpy("text/html"));
	}

	conn->cached = cached;
	abort_connection(conn, connection_state(S_OK));
}
