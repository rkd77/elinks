/* $Id: cgi.h,v 1.2 2005/04/13 17:08:59 jonas Exp $ */

#ifndef EL__PROTOCOL_FILE_CGI_H
#define EL__PROTOCOL_FILE_CGI_H

struct connection;

int execute_cgi(struct connection *);

#endif
