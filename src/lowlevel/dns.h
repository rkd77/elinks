/* $Id: dns.h,v 1.7 2004/10/07 03:15:15 jonas Exp $ */

#ifndef EL__LOWLEVEL_DNS_H
#define EL__LOWLEVEL_DNS_H

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

int do_real_lookup(unsigned char *, struct sockaddr_storage **, int *, int);
void shrink_dns_cache(int);
int find_host(unsigned char *, struct sockaddr_storage **, int *, void **, void (*)(void *, int), void *);
int find_host_no_cache(unsigned char *, struct sockaddr_storage **, int *, void **, void (*)(void *, int), void *);
void kill_dns_request(void **);

#endif
