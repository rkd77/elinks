/* $Id: header.h,v 1.3 2004/07/02 23:14:22 zas Exp $ */

#ifndef EL__PROTOCOL_HEADER_H
#define EL__PROTOCOL_HEADER_H

unsigned char *parse_header(unsigned char *, unsigned char *, unsigned char **);
unsigned char *parse_header_param(unsigned char *, unsigned char *);
unsigned char *get_header_param(unsigned char *, unsigned char *);

#endif
