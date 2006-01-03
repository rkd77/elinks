/* Provide stub function getifaddrs(). */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef HAVE_GETIFADDRS
/* Downloaded from http://ftp.uninett.no/pub/OpenBSD/src/kerberosV/src/lib/roken/getifaddrs.c */

/*
 * Copyright (c) 2000 - 2001 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* ELinksification and improvement of alloc failure handling by Zas.
 * This file was borrowed from dkftpbench-0.45 sources (http://www.kegel.com/dkftpbench). */

#include <errno.h>
#include <sys/types.h>		/* SunOS needs this before net/if.h */
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>		/* SunOS needs this before net/if.h, and need to be after sys/types.h */
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif
#ifdef HAVE_NETINET_IN6_VAR_H
#include <netinet/in6_var.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>		/* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#endif

#include "elinks.h"

#include "osdep/osdep.h"
#include "osdep/getifaddrs.h"


#if (defined(PF_INET6)    && defined(SIOCGIF6CONF) && defined(SIOCGIF6FLAGS)) \
 || (defined(PF_INET)     && defined(SIOCGIFCONF)  && defined(SIOCGIFFLAGS)) \
 || (defined(CONFIG_IPV6) && defined(SIOCGIFCONF))

static int
getifaddrs2(struct ifaddrs **ifap,
	    int pf, int siocgifconf, int siocgifflags, size_t ifreq_sz)
{
	struct sockaddr sa_zero;
	struct ifreq *ifr;
	struct ifaddrs *start;
	struct ifaddrs **end = &start;
	struct ifconf ifconf;
	char *p;
	char *buf = NULL;
	size_t buf_size;
	size_t sz;
	int ret;
	int fd;
	int num, j = 0;

	memset(&sa_zero, 0, sizeof(sa_zero));
	fd = socket(pf, SOCK_DGRAM, 0);
	if (fd < 0)
		return -1;

	buf_size = 8192;
	for (;;) {
		buf = calloc(1, buf_size);
		if (buf == NULL) {
			ret = ENOMEM;
			goto error_out;
		}
		ifconf.ifc_len = buf_size;
		ifconf.ifc_buf = buf;

		/*
		 * Solaris returns EINVAL when the buffer is too small.
		 */
		if (ioctl(fd, siocgifconf, &ifconf) < 0 && errno != EINVAL) {
			ret = errno;
			goto error_out;
		}
		/*
		 * Can the difference between a full and a overfull buf
		 * be determined?
		 */

		if (ifconf.ifc_len < (int) buf_size)
			break;

		free(buf);
		buf_size *= 2;
	}

	num = ifconf.ifc_len / ifreq_sz;
	j = 0;
	for (p = ifconf.ifc_buf; p < ifconf.ifc_buf + ifconf.ifc_len; p += sz) {
		struct ifreq ifreq;
		struct sockaddr *sa;
		size_t salen;
		void *tmp_end, *tmp_name, *tmp_addr;

		ifr = (struct ifreq *) p;
		sa = &ifr->ifr_addr;

		sz = ifreq_sz;
		salen = sizeof(*sa);
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
		salen = sa->sa_len;
		sz = int_max(sz, sizeof(ifr->ifr_name) + sa->sa_len);
#endif
#ifdef SA_LEN
		salen = SA_LEN(sa);
		sz = int_max(sz, sizeof(ifr->ifr_name) + SA_LEN(sa));
#endif
		memset(&ifreq, 0, sizeof(ifreq));
		memcpy(ifreq.ifr_name, ifr->ifr_name, sizeof(ifr->ifr_name));

		if (ioctl(fd, siocgifflags, &ifreq) < 0) {
			ret = errno;
			goto error_out;
		}

		tmp_end = malloc(sizeof(**end));
		if (!tmp_end) {
			ret = ENOMEM;
			goto error_out;
		}

		tmp_name = strdup(ifr->ifr_name);
		if (!tmp_name) {
			free(tmp_end);
			ret = ENOMEM;
			goto error_out;
		}

		tmp_addr = malloc(salen);
		if (!tmp_addr) {
			free(tmp_end);
			free(tmp_name);
			ret = ENOMEM;
			goto error_out;
		}

		*end = tmp_end;
		(*end)->ifa_next = NULL;
		(*end)->ifa_name = tmp_name;
		(*end)->ifa_flags = ifreq.ifr_flags;
		(*end)->ifa_addr = tmp_addr;
		memcpy((*end)->ifa_addr, sa, salen);
		(*end)->ifa_netmask = NULL;

#if 0
		/* fix these when we actually need them */
		if (ifreq.ifr_flags & IFF_BROADCAST) {
			(*end)->ifa_broadaddr =
			    malloc(sizeof(ifr->ifr_broadaddr));
			memcpy((*end)->ifa_broadaddr, &ifr->ifr_broadaddr,
			       sizeof(ifr->ifr_broadaddr));
		} else if (ifreq.ifr_flags & IFF_POINTOPOINT) {
			(*end)->ifa_dstaddr = malloc(sizeof(ifr->ifr_dstaddr));
			memcpy((*end)->ifa_dstaddr, &ifr->ifr_dstaddr,
			       sizeof(ifr->ifr_dstaddr));
		} else
			(*end)->ifa_dstaddr = NULL;
#else
		(*end)->ifa_dstaddr = NULL;
#endif

		(*end)->ifa_data = NULL;

		end = &(*end)->ifa_next;

	}

	*ifap = start;
	close(fd);
	free(buf);

	return 0;

error_out:
	close(fd);
	if (buf) free(buf);
	errno = ret;

	return -1;
}
#endif

int
getifaddrs(struct ifaddrs **ifap)
{
	int ret = -1;

	errno = ENXIO;

#if defined(PF_INET6) && defined(SIOCGIF6CONF) && defined(SIOCGIF6FLAGS)
	if (ret)
		ret = getifaddrs2(ifap, PF_INET6, SIOCGIF6CONF, SIOCGIF6FLAGS,
				  sizeof(struct in6_ifreq));
#endif
#if defined(CONFIG_IPV6) && defined(SIOCGIFCONF)
	if (ret)
		ret = getifaddrs2(ifap, PF_INET6, SIOCGIFCONF, SIOCGIFFLAGS,
				  sizeof(struct ifreq));
#endif
#if defined(PF_INET) && defined(SIOCGIFCONF) && defined(SIOCGIFFLAGS)
	if (ret)
		ret = getifaddrs2(ifap, PF_INET, SIOCGIFCONF, SIOCGIFFLAGS,
				  sizeof(struct ifreq));
#endif

	return ret;
}

void
freeifaddrs(struct ifaddrs *ifp)
{
	struct ifaddrs *p, *q;

	for (p = ifp; p;) {
		free(p->ifa_name);
		if (p->ifa_addr)
			free(p->ifa_addr);
		if (p->ifa_dstaddr)
			free(p->ifa_dstaddr);
		if (p->ifa_netmask)
			free(p->ifa_netmask);
		if (p->ifa_data)
			free(p->ifa_data);
		q = p;
		p = p->ifa_next;
		free(q);
	}
}

#ifdef TEST_GETIFADDRS

void
print_addr(const char *s, struct sockaddr *sa)
{
	int i;

	printf("  %s=%d/", s, sa->sa_family);
#ifdef HAVE_STRUCT_SOCKADDR_SA_LEN
	for (i = 0;
	     i < sa->sa_len - ((long) sa->sa_data - (long) &sa->sa_family); i++)
		printf("%02x", ((unsigned char *) sa->sa_data)[i]);
#else
	for (i = 0; i < sizeof(sa->sa_data); i++)
		printf("%02x", ((unsigned char *) sa->sa_data)[i]);
#endif
	printf("\n");
}

void
print_ifaddrs(struct ifaddrs *x)
{
	struct ifaddrs *p;

	for (p = x; p; p = p->ifa_next) {
		printf("%s\n", p->ifa_name);
		printf("  flags=%x\n", p->ifa_flags);
		if (p->ifa_addr)
			print_addr("addr", p->ifa_addr);
		if (p->ifa_dstaddr)
			print_addr("dstaddr", p->ifa_dstaddr);
		if (p->ifa_netmask)
			print_addr("netmask", p->ifa_netmask);
		printf("  %p\n", p->ifa_data);
	}
}

int
main()
{
	struct ifaddrs *a = NULL, *b;

	getifaddrs2(&a, PF_INET, SIOCGIFCONF, SIOCGIFFLAGS,
		    sizeof(struct ifreq));
	print_ifaddrs(a);
	printf("---\n");
	getifaddrs(&b);
	print_ifaddrs(b);
	return 0;
}

#endif	/* TEST_GETIFADDRS */

#endif	/* HAVE_GETIFADDRS */
