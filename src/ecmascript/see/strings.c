#include <see/see.h>

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

void
init_intern_strings(void)
{
	static SEE_char_t SA_window[] =    {'w','i','n','d','o','w'};
	static struct SEE_string S_window = SEE_STRING_DECL(SA_window);
	static SEE_char_t SA_closed[] =  {'c','l','o','s','e','d'};
	static struct SEE_string S_closed = SEE_STRING_DECL(SA_closed);
	static SEE_char_t SA_parent[] =  {'p','a','r','e','n','t'};
	static struct SEE_string S_parent = SEE_STRING_DECL(SA_parent);
	static SEE_char_t SA_self[] =  {'s','e','l','f'};
	static struct SEE_string S_self = SEE_STRING_DECL(SA_self);
	static SEE_char_t SA_top[] =  {'t','o','p'};
	static struct SEE_string S_top = SEE_STRING_DECL(SA_top);
	static SEE_char_t SA_alert[] ={'a','l','e','r','t'};
	static struct SEE_string S_alert = SEE_STRING_DECL(SA_alert);
	static SEE_char_t SA_open[] ={'o','p','e','n'};
	static struct SEE_string S_open = SEE_STRING_DECL(SA_open);

	static SEE_char_t SA_menubar[] =  {'m','e','n','u','b','a','r'};
	static struct SEE_string S_menubar = SEE_STRING_DECL(SA_menubar);

	static SEE_char_t SA_statusbar[] =  {'s','t','a','t','u','s','b','a','r'};
	static struct SEE_string S_statusbar = SEE_STRING_DECL(SA_statusbar);
	static SEE_char_t SA_visible[] =  {'v','i','s','i','b','l','e'};
	static struct SEE_string S_visible = SEE_STRING_DECL(SA_visible);

	static SEE_char_t SA_navigator[] ={'n','a','v','i','g','a','t','o','r'};
	static struct SEE_string S_navigator = SEE_STRING_DECL(SA_navigator);
	static SEE_char_t SA_appCodeName[] ={'a','p','p','C','o','d','e','N','a','m','e'};
	static struct SEE_string S_appCodeName = SEE_STRING_DECL(SA_appCodeName);
	static SEE_char_t SA_appName[] ={'a','p','p','N','a','m','e'};
	static struct SEE_string S_appName = SEE_STRING_DECL(SA_appName);
	static SEE_char_t SA_appVersion[] ={'a','p','p','V','e','r','s','i','o','n'};
	static struct SEE_string S_appVersion = SEE_STRING_DECL(SA_appVersion);
	static SEE_char_t SA_language[] ={'l','a','n','g','u','a','g','e'};
	static struct SEE_string S_language = SEE_STRING_DECL(SA_language);
	static SEE_char_t SA_platform[] ={'p','l','a','t','f','o','r','m'};
	static struct SEE_string S_platform = SEE_STRING_DECL(SA_platform);
	static SEE_char_t SA_userAgent[] ={'u','s','e','r','A','g','e','n','t'};
	static struct SEE_string S_userAgent = SEE_STRING_DECL(SA_userAgent);

	static SEE_char_t SA_history[] ={'h','i','s','t','o','r','y'};
	static struct SEE_string S_history = SEE_STRING_DECL(SA_history);
	static SEE_char_t SA_back[] ={'b','a','c','k'};
	static struct SEE_string S_back = SEE_STRING_DECL(SA_back);
	static SEE_char_t SA_forward[] ={'f','o','r','w','a','r','d'};
	static struct SEE_string S_forward = SEE_STRING_DECL(SA_forward);
	static SEE_char_t SA_go[] ={'g','o'};
	static struct SEE_string S_go = SEE_STRING_DECL(SA_go);

	static SEE_char_t SA_location[] ={'l','o','c','a','t','i','o','n'};
	static struct SEE_string S_location = SEE_STRING_DECL(SA_location);
	static SEE_char_t SA_href[] ={'h','r','e','f'};
	static struct SEE_string S_href = SEE_STRING_DECL(SA_href);
	static SEE_char_t SA_toString[] ={'t','o','S','t','r','i','n','g'};
	static struct SEE_string S_toString = SEE_STRING_DECL(SA_toString);
	static SEE_char_t SA_toLocaleString[] ={'t','o','L','o','c','a','l','e','S','t','r','i','n','g'};
	static struct SEE_string S_toLocaleString = SEE_STRING_DECL(SA_toLocaleString);

	static SEE_char_t SA_input[] ={'i','n','p','u','t'};
	static struct SEE_string S_input = SEE_STRING_DECL(SA_input);
	static SEE_char_t SA_accessKey[] ={'a','c','c','e','s','s','K','e','y'};
	static struct SEE_string S_accessKey = SEE_STRING_DECL(SA_accessKey);
	static SEE_char_t SA_alt[] ={'a','l','t'};
	static struct SEE_string S_alt = SEE_STRING_DECL(SA_alt);
	static SEE_char_t SA_checked[] ={'c','h','e','c','k','e','d'};
	static struct SEE_string S_checked = SEE_STRING_DECL(SA_checked);
	static SEE_char_t SA_defaultChecked[] ={'d','e','f','a','u','l','t','C','h','e','c','k','e','d'};
	static struct SEE_string S_defaultChecked = SEE_STRING_DECL(SA_defaultChecked);
	static SEE_char_t SA_defaultValue[] ={'d','e','f','a','u','l','t','V','a','l','u','e'};
	static struct SEE_string S_defaultValue = SEE_STRING_DECL(SA_defaultValue);
	static SEE_char_t SA_disabled[] ={'d','i','s','a','b','l','e','d'};
	static struct SEE_string S_disabled = SEE_STRING_DECL(SA_disabled);
	static SEE_char_t SA_form[] ={'f','o','r','m'};
	static struct SEE_string S_form = SEE_STRING_DECL(SA_form);
	static SEE_char_t SA_maxLength[] ={'m','a','x','L','e','n','g','t','h'};
	static struct SEE_string S_maxLength = SEE_STRING_DECL(SA_maxLength);
	static SEE_char_t SA_name[] ={'n','a','m','e'};
	static struct SEE_string S_name = SEE_STRING_DECL(SA_name);
	static SEE_char_t SA_readonly[] ={'r','e','a','d','o','n','l','y'};
	static struct SEE_string S_readonly = SEE_STRING_DECL(SA_readonly);
	static SEE_char_t SA_size[] ={'s','i','z','e'};
	static struct SEE_string S_size = SEE_STRING_DECL(SA_size);
	static SEE_char_t SA_src[] ={'s','r','c'};
	static struct SEE_string S_src = SEE_STRING_DECL(SA_src);
	static SEE_char_t SA_tabindex[] ={'t','a','b','i','n','d','e','x'};
	static struct SEE_string S_tabindex = SEE_STRING_DECL(SA_tabindex);
	static SEE_char_t SA_type[] ={'t','y','p','e'};
	static struct SEE_string S_type = SEE_STRING_DECL(SA_type);
	static SEE_char_t SA_value[] ={'v','a','l','u','e'};
	static struct SEE_string S_value = SEE_STRING_DECL(SA_value);
	static SEE_char_t SA_blur[] ={'b','l','u','r'};
	static struct SEE_string S_blur = SEE_STRING_DECL(SA_blur);
	static SEE_char_t SA_click[] ={'c','l','i','c','k'};
	static struct SEE_string S_click = SEE_STRING_DECL(SA_click);
	static SEE_char_t SA_focus[] ={'f','o','c','u','s'};
	static struct SEE_string S_focus = SEE_STRING_DECL(SA_focus);
	static SEE_char_t SA_select[] ={'s','e','l','e','c','t'};
	static struct SEE_string S_select = SEE_STRING_DECL(SA_select);

	static SEE_char_t SA_elements[] ={'e','l','e','m','e','n','t','s'};
	static struct SEE_string S_elements = SEE_STRING_DECL(SA_elements);
	static SEE_char_t SA_item[] ={'i','t','e','m'};
	static struct SEE_string S_item = SEE_STRING_DECL(SA_item);
	static SEE_char_t SA_namedItem[] ={'n','a','m','e','d','I','t','e','m'};
	static struct SEE_string S_namedItem = SEE_STRING_DECL(SA_namedItem);
	static SEE_char_t SA_length[] ={'l','e','n','g','t','h'};
	static struct SEE_string S_length = SEE_STRING_DECL(SA_length);

	static SEE_char_t SA_action[] ={'a','c','t','i','o','n'};
	static struct SEE_string S_action = SEE_STRING_DECL(SA_action);
	static SEE_char_t SA_encoding[] ={'e','n','c','o','d','i','g'};
	static struct SEE_string S_encoding = SEE_STRING_DECL(SA_encoding);
	static SEE_char_t SA_method[] ={'m','e','t','h','o','d'};
	static struct SEE_string S_method = SEE_STRING_DECL(SA_method);
	static SEE_char_t SA_target[] ={'t','a','r','g','e','t'};
	static struct SEE_string S_target = SEE_STRING_DECL(SA_target);
	static SEE_char_t SA_reset[] ={'r','e','s','e','t'};
	static struct SEE_string S_reset = SEE_STRING_DECL(SA_reset);
	static SEE_char_t SA_submit[] ={'s','u','b','m','i','t'};
	static struct SEE_string S_submit = SEE_STRING_DECL(SA_submit);

	static SEE_char_t SA_forms[] ={'f','o','r','m','s'};
	static struct SEE_string S_forms = SEE_STRING_DECL(SA_forms);

	static SEE_char_t SA_document[] = {'d','o','c','u','m','e','n','t'};
	static struct SEE_string S_document = SEE_STRING_DECL(SA_document);
	static SEE_char_t SA_referrer[] ={'r','e','f','e','r','r','e','r'};
	static struct SEE_string S_referrer = SEE_STRING_DECL(SA_referrer);
	static SEE_char_t SA_title[] ={'t','i','t','l','e'};
	static struct SEE_string S_title = SEE_STRING_DECL(SA_title);
	static SEE_char_t SA_url[] ={'u','r','l'};
	static struct SEE_string S_url = SEE_STRING_DECL(SA_url);
	static SEE_char_t SA_write[] = {'w','r','i','t','e'};
	static struct SEE_string S_write = SEE_STRING_DECL(SA_write);

	static SEE_char_t SA_Mozilla[] = {'M','o','z','i','l','l','a'};
	static struct SEE_string S_Mozilla = SEE_STRING_DECL(SA_Mozilla);
	static SEE_char_t SA_ELinks_[] = {'E','L','i','n','k','s',' ','(',
	 'r','o','u','g','h','l','y',' ','c','o','m','p','a','t','i','b','l','e',
	 ' ','w','i','t','h',' ','N','e','t','s','c','a','p','e',' ',
	 'N','a','v','i','g','a','t','o','r',',',' ','M','o','z','i','l','l','a',
	 ' ','a','n','d',' ','M','i','c','r','o','s','o','f','t',' ',
	 'I','n','t','e','r','n','e','t',' ','E','x','p','l','o','r','e','r',')'};
	static struct SEE_string S_ELinks_ = SEE_STRING_DECL(SA_ELinks_);

	static SEE_char_t SA_cookie[] = {'c','o','o','k','i','e'};
	static struct SEE_string S_cookie = SEE_STRING_DECL(SA_cookie);

	static SEE_char_t SA_GET[] = {'G','E','T'};
	static struct SEE_string S_GET = SEE_STRING_DECL(SA_GET);
	static SEE_char_t SA_POST[] = {'P','O','S','T'};
	static struct SEE_string S_POST = SEE_STRING_DECL(SA_POST);

	static SEE_char_t SA_application_[] = {'a','p','p','l','i','c','a','t',
	'i','o','n','/','x','-','w','w','w','-','f','o','r','m','-','u','r','l',
	'e','n','c','o','d','e','d'};
	static struct SEE_string S_application_ = SEE_STRING_DECL(SA_application_);
	static SEE_char_t SA_multipart_[] = {'m','u','l','t','i','p','a','r','t','/',
	'f','o','r','m','-','d','a','t','a'};
	static struct SEE_string S_multipart_ = SEE_STRING_DECL(SA_multipart_);
	static SEE_char_t SA_textplain[] = {'t','e','x','t','/','p','l','a','i','n'};
	static struct SEE_string S_textplain = SEE_STRING_DECL(SA_textplain);

	static SEE_char_t SA_text[] = {'t','e','x','t'};
	static struct SEE_string S_text = SEE_STRING_DECL(SA_text);

	static SEE_char_t SA_password[] = {'p','a','s','s','w','o','r','d'};
	static struct SEE_string S_password = SEE_STRING_DECL(SA_password);

	static SEE_char_t SA_file[] = {'f','i','l','e'};
	static struct SEE_string S_file = SEE_STRING_DECL(SA_file);

	static SEE_char_t SA_checkbox[] = {'c','h','e','c','k','b','o','x'};
	static struct SEE_string S_checkbox = SEE_STRING_DECL(SA_checkbox);

	static SEE_char_t SA_radio[] = {'r','a','d','i','o'};
	static struct SEE_string S_radio = SEE_STRING_DECL(SA_radio);

	static SEE_char_t SA_image[] = {'i','m','a','g','e'};
	static struct SEE_string S_image = SEE_STRING_DECL(SA_image);

	static SEE_char_t SA_button[] = {'b','u','t','t','o','n'};
	static struct SEE_string S_button = SEE_STRING_DECL(SA_button);

	static SEE_char_t SA_hidden[] = {'h','i','d','d','e','n'};
	static struct SEE_string S_hidden = SEE_STRING_DECL(SA_hidden);

	static SEE_char_t SA_timeout[] = {'t','i','m','e','o','u','t'};
	static struct SEE_string S_timeout = SEE_STRING_DECL(SA_timeout);

	SEE_intern_global(s_window = &S_window);
	SEE_intern_global(s_closed = &S_closed);
	SEE_intern_global(s_parent = &S_parent);
	SEE_intern_global(s_self = &S_self);
	SEE_intern_global(s_top = &S_top);
	SEE_intern_global(s_alert = &S_alert);
	SEE_intern_global(s_open = &S_open);

	SEE_intern_global(s_menubar = &S_menubar);
	SEE_intern_global(s_statusbar = &S_statusbar);
	SEE_intern_global(s_visible = &S_visible);

	SEE_intern_global(s_navigator = &S_navigator);
	SEE_intern_global(s_appCodeName = &S_appCodeName);
	SEE_intern_global(s_appName = &S_appName);
	SEE_intern_global(s_appVersion = &S_appVersion);
	SEE_intern_global(s_language = &S_language);
	SEE_intern_global(s_platform = &S_platform);
	SEE_intern_global(s_userAgent = &S_userAgent);

	SEE_intern_global(s_history = &S_history);
	SEE_intern_global(s_back = &S_back);
	SEE_intern_global(s_forward = &S_forward);
	SEE_intern_global(s_go = &S_go);

	SEE_intern_global(s_location = &S_location);
	SEE_intern_global(s_href = &S_href);
	SEE_intern_global(s_toString = &S_toString);
	SEE_intern_global(s_toLocaleString = &S_toLocaleString);

	SEE_intern_global(s_input = &S_input);
	SEE_intern_global(s_accessKey = &S_accessKey);
	SEE_intern_global(s_alt = &S_alt);
	SEE_intern_global(s_checked = &S_checked);
	SEE_intern_global(s_defaultChecked = &S_defaultChecked);
	SEE_intern_global(s_defaultValue = &S_defaultValue);
	SEE_intern_global(s_disabled = &S_disabled);
	SEE_intern_global(s_form = &S_form);
	SEE_intern_global(s_maxLength = &S_maxLength);
	SEE_intern_global(s_name = &S_name);
	SEE_intern_global(s_readonly = &S_readonly);
	SEE_intern_global(s_size = &S_size);
	SEE_intern_global(s_src = &S_src);
	SEE_intern_global(s_tabindex = &S_tabindex);
	SEE_intern_global(s_type = &S_type);
	SEE_intern_global(s_value = &S_value);
	SEE_intern_global(s_blur = &S_blur);
	SEE_intern_global(s_click = &S_click);
	SEE_intern_global(s_focus = &S_focus);
	SEE_intern_global(s_select = &S_select);

	SEE_intern_global(s_elements = &S_elements);
	SEE_intern_global(s_item = &S_item);
	SEE_intern_global(s_namedItem = &S_namedItem);
	SEE_intern_global(s_length = &S_length);

	SEE_intern_global(s_action = &S_action);
	SEE_intern_global(s_encoding = &S_encoding);
	SEE_intern_global(s_method = &S_method);
	SEE_intern_global(s_target = &S_target);
	SEE_intern_global(s_reset = &S_reset);
	SEE_intern_global(s_submit = &S_submit);

	SEE_intern_global(s_forms = &S_forms);

	SEE_intern_global(s_document = &S_document);
	SEE_intern_global(s_referrer = &S_referrer);
	SEE_intern_global(s_title = &S_title);
	SEE_intern_global(s_url = &S_url);
	SEE_intern_global(s_write = &S_write);

	SEE_intern_global(s_Mozilla = &S_Mozilla);
	SEE_intern_global(s_ELinks_ = &S_ELinks_);
	SEE_intern_global(s_cookie = &S_cookie);

	SEE_intern_global(s_GET = &S_GET);
	SEE_intern_global(s_POST = &S_POST);

	SEE_intern_global(s_application_ = &S_application_);
	SEE_intern_global(s_multipart_ = &S_multipart_);
	SEE_intern_global(s_textplain = &S_textplain);

	SEE_intern_global(s_text = &S_text);
	SEE_intern_global(s_password = &S_password);
	SEE_intern_global(s_file = &S_file);
	SEE_intern_global(s_checkbox = &S_checkbox);
	SEE_intern_global(s_radio = &S_radio);
	SEE_intern_global(s_image = &S_image);
	SEE_intern_global(s_button = &S_button);
	SEE_intern_global(s_hidden = &S_hidden);

	SEE_intern_global(s_timeout = &S_timeout);
}
