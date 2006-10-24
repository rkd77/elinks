
#ifndef EL__ECMASCRIPT_SEE_STRINGS_H
#define EL__ECMASCRIPT_SEE_STRINGS_H

struct SEE_string;

void init_intern_strings(void);

extern struct SEE_string *s_window;
extern struct SEE_string *s_closed;
extern struct SEE_string *s_parent;
extern struct SEE_string *s_self;
extern struct SEE_string *s_top;
extern struct SEE_string *s_alert;
extern struct SEE_string *s_open;

extern struct SEE_string *s_menubar;

extern struct SEE_string *s_statusbar;
extern struct SEE_string *s_visible;

extern struct SEE_string *s_navigator;
extern struct SEE_string *s_appCodeName;
extern struct SEE_string *s_appName;
extern struct SEE_string *s_appVersion;
extern struct SEE_string *s_language;
extern struct SEE_string *s_platform;
extern struct SEE_string *s_userAgent;

extern struct SEE_string *s_history;
extern struct SEE_string *s_back;
extern struct SEE_string *s_forward;
extern struct SEE_string *s_go;

extern struct SEE_string *s_location;
extern struct SEE_string *s_href;
extern struct SEE_string *s_toString;
extern struct SEE_string *s_toLocaleString;

extern struct SEE_string *s_input;
extern struct SEE_string *s_accessKey;
extern struct SEE_string *s_alt;
extern struct SEE_string *s_checked;
extern struct SEE_string *s_defaultChecked;
extern struct SEE_string *s_defaultValue;
extern struct SEE_string *s_disabled;
extern struct SEE_string *s_form;
extern struct SEE_string *s_maxLength;
extern struct SEE_string *s_name;
extern struct SEE_string *s_readonly;
extern struct SEE_string *s_size;
extern struct SEE_string *s_src;
extern struct SEE_string *s_tabindex;
extern struct SEE_string *s_type;
extern struct SEE_string *s_value;
extern struct SEE_string *s_blur;
extern struct SEE_string *s_click;
extern struct SEE_string *s_focus;
extern struct SEE_string *s_select;
extern struct SEE_string *s_selectedIndex;

extern struct SEE_string *s_elements;
extern struct SEE_string *s_item;
extern struct SEE_string *s_namedItem;
extern struct SEE_string *s_length;

extern struct SEE_string *s_action;
extern struct SEE_string *s_encoding;
extern struct SEE_string *s_method;
extern struct SEE_string *s_target;
extern struct SEE_string *s_reset;
extern struct SEE_string *s_submit;

extern struct SEE_string *s_forms;

extern struct SEE_string *s_document;
extern struct SEE_string *s_referrer;
extern struct SEE_string *s_title;
extern struct SEE_string *s_url;
extern struct SEE_string *s_write;
extern struct SEE_string *s_writeln;

extern struct SEE_string *s_Mozilla;
extern struct SEE_string *s_ELinks_;
extern struct SEE_string *s_cookie;

extern struct SEE_string *s_GET;
extern struct SEE_string *s_POST;
extern struct SEE_string *s_application_;
extern struct SEE_string *s_multipart_;
extern struct SEE_string *s_textplain;

extern struct SEE_string *s_text;
extern struct SEE_string *s_password;
extern struct SEE_string *s_file;
extern struct SEE_string *s_checkbox;
extern struct SEE_string *s_radio;
extern struct SEE_string *s_image;
extern struct SEE_string *s_button;
extern struct SEE_string *s_hidden;

extern struct SEE_string *s_timeout;
extern struct SEE_string *s_setTimeout;
extern struct SEE_string *s_status;
#endif
