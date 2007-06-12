#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "dom/node.h"

#include "document/dom/ecmascript/spidermonkey/html/HTMLAnchorElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLAppletElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLAreaElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLBaseElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLBaseFontElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLBodyElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLBRElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLButtonElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLCollection.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLDirectoryElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLDivElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLDListElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLDocument.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFieldSetElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFontElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFormElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFrameElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLFrameSetElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLHeadElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLHeadingElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLHRElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLHtmlElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLIFrameElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLImageElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLInputElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLIsIndexElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLLabelElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLLegendElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLLIElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLLinkElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLMapElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLMenuElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLMetaElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLModElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLObjectElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLOListElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLOptGroupElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLOptionElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLOptionsCollection.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLParagraphElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLParamElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLPreElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLQuoteElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLScriptElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLSelectElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLStyleElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableCaptionElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableCellElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableColElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableRowElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTableSectionElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTextAreaElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLTitleElement.h"
#include "document/dom/ecmascript/spidermonkey/html/HTMLUListElement.h"
#include "dom/sgml/html/html.h"
#include "ecmascript/spidermonkey/util.h"

typedef void (make_function_T)(JSContext *ctx, struct dom_node *node);
typedef void (done_function_T)(struct dom_node *node);

struct HTML_functions {
	make_function_T *make;
	done_function_T *done;
};

#define HTML_(node,name,id)	{ make##_##name##_object, done##_##name##_object }

struct HTML_functions func[HTML_ELEMENTS] = {
{ NULL, NULL },
#include "dom/sgml/html/element.inc"
};

void
done_dom_node_html_data(struct dom_node *node)
{
	int type = node->data.element.type;
	void *data = node->data.element.html_data;

	if (func[type].done)
		func[type].done(node);
	done_HTMLElement(node);
	mem_free(data);
}

#if 0
void
make_dom_node_html_data(, struct dom_node *node)
{
	int type = node->data.element.type;

	if (func[type].make)
		func[type].make(ctx, node);
}
#endif
