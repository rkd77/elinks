#include <see/see.h>
#include "ecmascript/see/strings.h"

struct SEE_string *s_window;
struct SEE_string *s_closed;
struct SEE_string *s_parent;
struct SEE_string *s_self;
struct SEE_string *s_top;
struct SEE_string *s_alert;
struct SEE_string *s_open;

struct SEE_string *s_menubar;

struct SEE_string *s_statusbar;
struct SEE_string *s_visible;

struct SEE_string *s_navigator;
struct SEE_string *s_appCodeName;
struct SEE_string *s_appName;
struct SEE_string *s_appVersion;
struct SEE_string *s_language;
struct SEE_string *s_platform;
struct SEE_string *s_userAgent;

struct SEE_string *s_history;
struct SEE_string *s_back;
struct SEE_string *s_forward;
struct SEE_string *s_go;

struct SEE_string *s_location;
struct SEE_string *s_href;
struct SEE_string *s_toString;
struct SEE_string *s_toLocaleString;

struct SEE_string *s_input;
struct SEE_string *s_accessKey;
struct SEE_string *s_alt;
struct SEE_string *s_checked;
struct SEE_string *s_defaultChecked;
struct SEE_string *s_defaultValue;
struct SEE_string *s_disabled;
struct SEE_string *s_form;
struct SEE_string *s_maxLength;
struct SEE_string *s_name;
struct SEE_string *s_readonly;
struct SEE_string *s_size;
struct SEE_string *s_src;
struct SEE_string *s_tabindex;
struct SEE_string *s_type;
struct SEE_string *s_value;
struct SEE_string *s_blur;
struct SEE_string *s_click;
struct SEE_string *s_focus;
struct SEE_string *s_select;
struct SEE_string *s_selectedIndex;

struct SEE_string *s_elements;
struct SEE_string *s_item;
struct SEE_string *s_namedItem;
struct SEE_string *s_length;

struct SEE_string *s_action;
struct SEE_string *s_encoding;
struct SEE_string *s_method;
struct SEE_string *s_target;
struct SEE_string *s_reset;
struct SEE_string *s_submit;

struct SEE_string *s_forms;

struct SEE_string *s_document;
struct SEE_string *s_referrer;
struct SEE_string *s_title;
struct SEE_string *s_url;
struct SEE_string *s_write;
struct SEE_string *s_writeln;

struct SEE_string *s_Mozilla;
struct SEE_string *s_ELinks_;
struct SEE_string *s_cookie;

struct SEE_string *s_GET;
struct SEE_string *s_POST;
struct SEE_string *s_application_;
struct SEE_string *s_multipart_;
struct SEE_string *s_textplain;

struct SEE_string *s_text;
struct SEE_string *s_password;
struct SEE_string *s_file;
struct SEE_string *s_checkbox;
struct SEE_string *s_radio;
struct SEE_string *s_image;
struct SEE_string *s_button;
struct SEE_string *s_hidden;

struct SEE_string *s_timeout;
struct SEE_string *s_setTimeout;
struct SEE_string *s_status;

void
init_intern_strings(void)
{
	s_window = SEE_intern_global("window");
	s_closed = SEE_intern_global("closed");
	s_parent = SEE_intern_global("parent");
	s_self = SEE_intern_global("self");
	s_top = SEE_intern_global("top");
	s_alert = SEE_intern_global("alert");
	s_open = SEE_intern_global("open");

	s_menubar = SEE_intern_global("menubar");

	s_statusbar = SEE_intern_global("statusbar");
	s_visible = SEE_intern_global("visible");

	s_navigator = SEE_intern_global("navigator");
	s_appCodeName = SEE_intern_global("appCodeName");
	s_appName = SEE_intern_global("appName");
	s_appVersion = SEE_intern_global("appVersion");
	s_language = SEE_intern_global("language");
	s_platform = SEE_intern_global("platform");
	s_userAgent = SEE_intern_global("userAgent");

	s_history = SEE_intern_global("history");
	s_back = SEE_intern_global("back");
	s_forward = SEE_intern_global("forward");
	s_go = SEE_intern_global("go");

	s_location = SEE_intern_global("location");
	s_href = SEE_intern_global("href");
	s_toString = SEE_intern_global("toString");
	s_toLocaleString = SEE_intern_global("toLocaleString");

	s_input = SEE_intern_global("input");
	s_accessKey = SEE_intern_global("accessKey");
	s_alt = SEE_intern_global("alt");
	s_checked = SEE_intern_global("checked");
	s_defaultChecked = SEE_intern_global("defaultChecked");
	s_defaultValue = SEE_intern_global("defaultValue");
	s_disabled = SEE_intern_global("disabled");
	s_form = SEE_intern_global("form");
	s_maxLength = SEE_intern_global("maxLength");
	s_name = SEE_intern_global("name");
	s_readonly = SEE_intern_global("readonly");
	s_size = SEE_intern_global("size");
	s_src = SEE_intern_global("src");
	s_tabindex = SEE_intern_global("tabindex");
	s_type = SEE_intern_global("type");
	s_value = SEE_intern_global("value");
	s_blur = SEE_intern_global("blur");
	s_click = SEE_intern_global("click");
	s_focus = SEE_intern_global("focus");
	s_select = SEE_intern_global("select");
	s_selectedIndex = SEE_intern_global("selectedIndex");

	s_elements = SEE_intern_global("elements");
	s_item = SEE_intern_global("item");
	s_namedItem = SEE_intern_global("namedItem");
	s_length = SEE_intern_global("length");

	s_action = SEE_intern_global("action");
	s_encoding = SEE_intern_global("encoding");
	s_method = SEE_intern_global("method");
	s_target = SEE_intern_global("target");
	s_reset = SEE_intern_global("reset");
	s_submit = SEE_intern_global("submit");

	s_forms = SEE_intern_global("forms");

	s_document = SEE_intern_global("document");
	s_referrer = SEE_intern_global("referrer");
	s_title = SEE_intern_global("title");
	s_url = SEE_intern_global("url");
	s_write = SEE_intern_global("write");
	s_writeln = SEE_intern_global("writeln");

	s_Mozilla = SEE_intern_global("Mozilla");
	s_ELinks_ = SEE_intern_global("ELinks (roughly compatible with Netscape Navigator Mozilla and Microsoft Internet Explorer)");
	s_cookie = SEE_intern_global("cookie");

	s_GET = SEE_intern_global("GET");
	s_POST = SEE_intern_global("POST");
	s_application_ = SEE_intern_global("application/x-www-form-urlencoded");
	s_multipart_ = SEE_intern_global("multipart/form-data");
	s_textplain = SEE_intern_global("text/plain");

	s_text = SEE_intern_global("text");
	s_password = SEE_intern_global("password");
	s_file = SEE_intern_global("file");
	s_checkbox = SEE_intern_global("checkbox");
	s_radio = SEE_intern_global("radio");
	s_image = SEE_intern_global("image");
	s_button = SEE_intern_global("button");
	s_hidden = SEE_intern_global("hidden");

	s_timeout = SEE_intern_global("timeout");
	s_setTimeout = SEE_intern_global("setTimeout");
	s_status = SEE_intern_global("status");
}
