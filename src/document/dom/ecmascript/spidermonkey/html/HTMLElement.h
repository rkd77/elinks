#ifndef EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLELEMENT_H
#define EL__DOCUMENT_DOM_ECMASCRIPT_SPIDERMONKEY_HTML_HTMLELEMENT_H

#include "ecmascript/spidermonkey/util.h"
struct dom_node;
struct hash;

extern const JSClass HTMLElement_class;
extern const JSFunctionSpec HTMLElement_funcs[];
extern const JSPropertySpec HTMLElement_props[];

JSBool HTMLElement_getProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);
JSBool HTMLElement_setProperty(JSContext *ctx, JSObject *obj, jsval id, jsval *vp);

struct HTMLElement_struct {
	unsigned char *id;
	unsigned char *title;
	unsigned char *lang;
	unsigned char *dir;
	unsigned char *class_name;
};

struct html_objects { /* FIXME: Better name for this type. */
	struct dom_node *document;
	JSObject *Node_object;
	JSObject *Element_object;
	JSObject *HTMLElement_object;
	struct hash *ids;
};

void make_HTMLElement(JSContext *ctx, struct dom_node *node);
void done_HTMLElement(struct dom_node *node);

#define make_ABBR_object make_HTMLElement
#define make_ACRONYM_object make_HTMLElement
#define make_ADDRESS_object make_HTMLElement
#define make_B_object make_HTMLElement
#define make_BDO_object make_HTMLElement
#define make_BIG_object make_HTMLElement
#define make_BLOCKQUOTE_object make_HTMLElement
#define make_CENTER_object make_HTMLElement
#define make_CITE_object make_HTMLElement
#define make_CODE_object make_HTMLElement
#define make_COLGROUP_object make_HTMLElement
#define make_DD_object make_HTMLElement
#define make_DEL_object make_HTMLElement
#define make_DFN_object make_HTMLElement
#define make_DT_object make_HTMLElement
#define make_EM_object make_HTMLElement
#define make_I_object make_HTMLElement
#define make_INS_object make_HTMLElement
#define make_KBD_object make_HTMLElement
#define make_NOFRAMES_object make_HTMLElement
#define make_NOSCRIPT_object make_HTMLElement
#define make_Q_object make_HTMLElement
#define make_S_object make_HTMLElement
#define make_SAMP_object make_HTMLElement
#define make_SMALL_object make_HTMLElement
#define make_SPAN_object make_HTMLElement
#define make_STRIKE_object make_HTMLElement
#define make_STRONG_object make_HTMLElement
#define make_SUB_object make_HTMLElement
#define make_SUP_object make_HTMLElement
#define make_TH_object make_HTMLElement
#define make_TT_object make_HTMLElement
#define make_U_object make_HTMLElement
#define make_VAR_object make_HTMLElement
#define make_XMP_object make_HTMLElement

#define done_ABBR_object NULL
#define done_ACRONYM_object NULL
#define done_ADDRESS_object NULL
#define done_B_object NULL
#define done_BDO_object NULL
#define done_BIG_object NULL
#define done_BLOCKQUOTE_object NULL
#define done_CENTER_object NULL
#define done_CITE_object NULL
#define done_CODE_object NULL
#define done_COLGROUP_object NULL
#define done_DD_object NULL
#define done_DEL_object NULL
#define done_DFN_object NULL
#define done_DT_object NULL
#define done_EM_object NULL
#define done_I_object NULL
#define done_INS_object NULL
#define done_KBD_object NULL
#define done_NOFRAMES_object NULL
#define done_NOSCRIPT_object NULL
#define done_Q_object NULL
#define done_S_object NULL
#define done_SAMP_object NULL
#define done_SMALL_object NULL
#define done_SPAN_object NULL
#define done_STRIKE_object NULL
#define done_STRONG_object NULL
#define done_SUB_object NULL
#define done_SUP_object NULL
#define done_TH_object NULL
#define done_TT_object NULL
#define done_U_object NULL
#define done_VAR_object NULL
#define done_XMP_object NULL

#endif
