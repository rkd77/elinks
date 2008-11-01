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
#include "document/html/parser/stack.h"
#include "document/html/parser/parse.h"
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
		if (c_strlcasecmp(element->name, element->namelen, name, namelen))
			continue;
		return element;
	}

	return NULL;
}


void
kill_html_stack_item(struct html_context *html_context, struct html_element *e)
{
#ifdef CONFIG_ECMASCRIPT
	unsigned char *onload = NULL;
#endif

	assert(e);
	if_assert_failed return;
	assertm((void *) e != &html_context->stack, "trying to free bad html element");
	if_assert_failed return;
	assertm(e->type != ELEMENT_IMMORTAL, "trying to kill unkillable element");
	if_assert_failed return;

#ifdef CONFIG_ECMASCRIPT
	/* As our another tiny l33t extension, we allow the onLoad attribute for
	 * any element, executing it when that element is fully loaded. */
	if (e->options)
		onload = get_attr_val(e->options, "onLoad",
		                     html_context->doc_cp);
	if (html_context->part
	    && html_context->part->document
	    && onload && *onload && *onload != '^') {
		/* XXX: The following expression alone amounts two #includes. */
		add_to_string_list(&html_context->part->document->onload_snippets,
		                   onload, -1);
	}
	if (onload) mem_free(onload);
#endif

	mem_free_if(e->attr.link);
	mem_free_if(e->attr.target);
	mem_free_if(e->attr.image);
	mem_free_if(e->attr.title);
	mem_free_if(e->attr.select);

#ifdef CONFIG_CSS
	mem_free_if(e->attr.id);
	mem_free_if(e->attr.class);
#endif

	mem_free_if(e->attr.onclick);
	mem_free_if(e->attr.ondblclick);
	mem_free_if(e->attr.onmouseover);
	mem_free_if(e->attr.onhover);
	mem_free_if(e->attr.onfocus);
	mem_free_if(e->attr.onmouseout);
	mem_free_if(e->attr.onblur);

	del_from_list(e);
	mem_free(e);
#if 0
	if (list_empty(html_context->stack)
	    || !html_context->stack.next) {
		DBG("killing last element");
	}
#endif
}


void
html_stack_dup(struct html_context *html_context, enum html_element_mortality_type type)
{
	struct html_element *e;
	struct html_element *ep = html_context->stack.next;

	assertm(ep && (void *) ep != &html_context->stack, "html stack empty");
	if_assert_failed return;

	e = mem_alloc(sizeof(*e));
	if (!e) return;

	copy_struct(e, ep);

	if (ep->attr.link) e->attr.link = stracpy(ep->attr.link);
	if (ep->attr.target) e->attr.target = stracpy(ep->attr.target);
	if (ep->attr.image) e->attr.image = stracpy(ep->attr.image);
	if (ep->attr.title) e->attr.title = stracpy(ep->attr.title);
	if (ep->attr.select) e->attr.select = stracpy(ep->attr.select);

#ifdef CONFIG_CSS
	e->attr.id = e->attr.class = NULL;
#endif
	/* We don't want to propagate these. */
	/* XXX: For sure? --pasky */
	e->attr.onclick = e->attr.ondblclick = e->attr.onmouseover = e->attr.onhover
		= e->attr.onfocus = e->attr.onmouseout = e->attr.onblur = NULL;

#if 0
	if (e->name) {
		if (e->attr.link) set_mem_comment(e->attr.link, e->name, e->namelen);
		if (e->attr.target) set_mem_comment(e->attr.target, e->name, e->namelen);
		if (e->attr.image) set_mem_comment(e->attr.image, e->name, e->namelen);
		if (e->attr.title) set_mem_comment(e->attr.title, e->name, e->namelen);
		if (e->attr.select) set_mem_comment(e->attr.select, e->name, e->namelen);
	}
#endif

	e->name = e->options = NULL;
	e->namelen = 0;
	e->type = type;

	add_to_list(html_context->stack, e);
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

			if (c_strlcasecmp(e->name, e->namelen, s, slen))
				continue;

			if (!sk) {
				if (e->type < ELEMENT_KILLABLE) break;
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

		if (e->type < ELEMENT_KILLABLE
		    || (!c_strlcasecmp(e->name, e->namelen, "TABLE", 5)))
			break;

		if (e->namelen == 2 && c_toupper(e->name[0]) == 'T') {
			unsigned char c = c_toupper(e->name[1]);

			if (c == 'D' || c == 'H' || c == 'R')
				break;
		}

		e = e->next;
	}
}
