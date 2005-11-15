#ifndef EL__COOKIES_PARSER_H
#define EL__COOKIES_PARSER_H

struct cookie_str {
	unsigned char *str;
	unsigned char *nam_end, *val_start, *val_end;
};

struct cookie_str *parse_cookie_str(struct cookie_str *cstr, unsigned char *str);

#endif
