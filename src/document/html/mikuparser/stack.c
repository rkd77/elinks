/* HTML elements stack */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/document.h"
#include "document/html/mikuparser/stack.h"
#include "document/html/mikuparser/parse.h"
#include "document/html/mikuparser/mikuparser.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "protocol/uri.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/lists.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"


#if 0
void
dump_html_stack(struct html_context *html_context)
{
	struct html_element *element;

	DBG("HTML stack debug:");
	foreach (element, html_context->stack) {
		DBG("&name/len:%p:%d name:%.*s type:%d",
		    element->name, element->namelen,
		    element->namelen, element->name,
		    element->type);
	}
	WDBG("Did you enjoy it?");
}
#endif


struct html_element *
search_html_stack(struct html_context *html_context, unsigned char *name)
{
	struct html_element *element;
	int namelen;

	assert(name && *name);
	namelen = strlen(name);

#if 0	/* Debug code. Please keep. */
	dump_html_stack(html_context);
#endif

	foreach (element, html_context->stack) {
		if (element == html_top)
			continue; /* skip the top element */
		if (strlcasecmp(element->name, element->namelen, name, namelen))
			continue;
		return element;
	}

	return NULL;
}


void
kill_html_stack_item(struct html_context *html_context, struct html_element *e)
{
	assert(e);
	if_assert_failed return;
	assertm(miku_el(e)->type != ELEMENT_IMMORTAL, "trying to kill unkillable element");
	if_assert_failed return;

	done_html_element(html_context, e);
}


void
html_stack_dup(struct html_context *html_context, enum html_element_mortality_type type)
{
	struct html_element *e;
	struct html_element *ep = html_context->stack.next;

	e = dup_html_element(html_context);
	if (!e) return;

	e->data = mem_alloc(sizeof(*miku_el(e)));
	copy_struct(miku_el(e), miku_el(ep));

	miku_el(e)->options = NULL;
	miku_el(e)->type = type;
}

static void
kill_element(struct html_context *html_context, int ls, struct html_element *e)
{
	int l = 0;

	while ((void *) e != &html_context->stack) {
		if (ls && e == html_context->stack.next)
			break;

		if (e->linebreak > l)
			l = e->linebreak;
		e = e->prev;
		kill_html_stack_item(html_context, e->next);
	}

	ln_break(html_context, l);
}

void
kill_html_stack_until(struct html_context *html_context, int ls, ...)
{
	struct html_element *e = html_top;

	if (ls) e = e->next;

	while ((void *) e != &html_context->stack) {
		int sk = 0;
		va_list arg;

		va_start(arg, ls);
		while (1) {
			unsigned char *s = va_arg(arg, unsigned char *);
			int slen;

			if (!s) break;

			slen = strlen(s);
			if (!slen) {
				sk++;
				continue;
			}

			if (strlcasecmp(e->name, e->namelen, s, slen))
				continue;

			if (!sk) {
				if (miku_el(e)->type < ELEMENT_KILLABLE) break;
				va_end(arg);
				kill_element(html_context, ls, e);
				return;

			} else if (sk == 1) {
				va_end(arg);
				e = e->prev;
				kill_element(html_context, ls, e);
				return;

			} else {
				break;
			}
		}
		va_end(arg);

		if (miku_el(e)->type < ELEMENT_KILLABLE
		    || (!strlcasecmp(e->name, e->namelen, "TABLE", 5)))
			break;

		if (e->namelen == 2 && toupper(e->name[0]) == 'T') {
			unsigned char c = toupper(e->name[1]);

			if (c == 'D' || c == 'H' || c == 'R')
				break;
		}

		e = e->next;
	}
}
