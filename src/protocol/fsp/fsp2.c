#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

/*
This file is part of fsplib - FSP protocol stack implemented in C
language. See http://fsp.sourceforge.net for more information.

Copyright (c) 2003-2005 by Radim HSN Kolar (hsn@sendmail.cz)

You may copy or modify this file in any manner you wish, provided
that this notice is always included, and that you hold the author
harmless for any loss or damage resulting from the installation or
use of this software.

                     This is a free software.  Be creative.
                    Let me know of any bugs and suggestions.
*/                  
#ifdef CONFIG_OS_WIN32
#define _CRT_RAND_S
#endif

#include <sys/types.h>

#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h>
#endif

#include <sys/time.h>

#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h>
#endif

#ifdef HAVE_WS2TCPIP_H
#include <ws2tcpip.h>
#endif

#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif

#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif

#include "elinks.h"

#include "intl/libintl.h"
#include "main/select.h"
#include "main/timer.h"
#include "network/connection.h"
#include "protocol/auth/auth.h"
#include "protocol/common.h"
#include "protocol/fsp/fsplib.h"
#include "protocol/fsp/lock.h"
#include "protocol/uri.h"
#include "util/conv.h"

#ifdef CONFIG_OS_WIN32
#undef random
long
random(void)
{
	ELOG
	unsigned int number = 0;
	(void)rand_s(&number);
	return (long)number;
}
#endif

static void fsp_stat_continue(void *data);

typedef void (*return_handler_T)(void *data);

struct fsp_connection_info {
	FSP_SESSION *ses;
	FSP_PKT in, out;
	FSP_FILE *f;
	FSP_DIR *dir;
	char *path;
	return_handler_T return_func;
	struct stat sb;
	timer_id_T send_timer;
	int ret;

	char buf[FSP_MAXPACKET];
	struct timeval start[8],stop;
	unsigned int retry,dupes;
	int w_delay; /* how long to wait on next packet */
	int f_delay; /* how long to wait after first send */
	int l_delay; /* last delay */
	unsigned int t_delay; /* time from first send */
	int blocksize;
	int pos;
};

struct module fsp_protocol_module = struct_module(
	/* name: */		N_("FSP"),
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL,
	/* getname: */	NULL
);

/* ************ Internal functions **************** */ 

/* builds filename in packet output buffer, appends password if needed */
static int
buildfilename(const FSP_SESSION *s, FSP_PKT *out, const char *dirname)
{
	ELOG
	int len;

	len = strlen(dirname);

	if (len >= FSP_SPACE - 1) {
		errno = ENAMETOOLONG;
		return -1;
	}
	/* copy name + \0 */
	memcpy(out->buf, dirname, len + 1);
	out->len = len;

	if (s->password) {
		out->buf[len]='\n';
		out->len++;
		len = strlen(s->password);

		if (out->len + len >= FSP_SPACE - 1) {
			errno = ENAMETOOLONG;
			return -1;
		}
		memcpy(out->buf + out->len, s->password, len + 1);
		out->len += len;
	}
	/* add terminating \0 */
	out->len++;

	return 0;
}

/* ************  Packet encoding / decoding *************** */

/* write binary representation of FSP packet p into *space. */
/* returns number of bytes used or zero on error            */
/* Space must be long enough to hold created packet.        */
/* Maximum created packet size is FSP_MAXPACKET             */

static size_t
fsp_pkt_write(const FSP_PKT *p, void *space)
{
	ELOG
	size_t used;
	unsigned char *ptr;
	int checksum;
	size_t i;

	if (p->xlen + p->len > FSP_SPACE) {
		/* not enough space */
		errno = EMSGSIZE;
		return 0;
	}
	ptr = space;

	/* pack header */
	ptr[FSP_OFFSET_CMD] = p->cmd;
	ptr[FSP_OFFSET_SUM] = 0;
	*(uint16_t *)(ptr + FSP_OFFSET_KEY) = htons(p->key);
	*(uint16_t *)(ptr + FSP_OFFSET_SEQ) = htons(p->seq);
	*(uint16_t *)(ptr + FSP_OFFSET_LEN) = htons(p->len);
	*(uint32_t *)(ptr + FSP_OFFSET_POS) = htonl(p->pos);
	used = FSP_HSIZE;
	/* copy data block */
	memcpy(ptr + FSP_HSIZE, p->buf, p->len);
	used += p->len;
	/* copy extra data block */
	memcpy(ptr + used, p->buf + p->len, p->xlen);
	used += p->xlen;
	/* compute checksum */
	checksum = 0;
	for(i = 0; i < used; i++) {
		checksum += ptr[i];
	}
	checksum +=used;
	ptr[FSP_OFFSET_SUM] =  checksum + (checksum >> 8);

	return used;
}

/* read binary representation of FSP packet received from network into p  */
/* return zero on success */
static int
fsp_pkt_read(FSP_PKT *p, const void *space, size_t recv_len)
{
	ELOG
	int mysum;
	size_t i;
	const unsigned char *ptr;

	if (recv_len < FSP_HSIZE) {
		/* too short */
		errno = ERANGE;
		return -1;
	}

	if (recv_len > FSP_MAXPACKET) {
		/* too long */
		errno = EMSGSIZE;
		return -1;
	}
	ptr = space;
	/* check sum */
	mysum = -ptr[FSP_OFFSET_SUM];

	for(i = 0; i < recv_len; i++) {
		mysum+=ptr[i];
	}
	mysum = (mysum + (mysum >> 8)) & 0xff;

	if (mysum != ptr[FSP_OFFSET_SUM]) {
		/* checksum failed */
#ifdef MAINTAINER_MODE
		fprintf(stderr, "mysum: %x, got %x\n", mysum, ptr[FSP_OFFSET_SUM]);
#endif
		errno = EIO;
		return -1;
	}

	/* unpack header */
	p->cmd = ptr[FSP_OFFSET_CMD];
	p->sum = mysum;
	p->key = ntohs(*(const uint16_t *)(ptr + FSP_OFFSET_KEY));
	p->seq = ntohs(*(const uint16_t *)(ptr + FSP_OFFSET_SEQ));
	p->len = ntohs(*(const uint16_t *)(ptr + FSP_OFFSET_LEN));
	p->pos = ntohl(*(const uint32_t *)(ptr + FSP_OFFSET_POS));

	if (p->len > recv_len) {
		/* bad length field, should not never happen */
		errno = EMSGSIZE;
		return -1;
	}
	p->xlen = recv_len - p->len - FSP_HSIZE;
	/* now copy data */
	memcpy(p->buf, ptr + FSP_HSIZE, recv_len - FSP_HSIZE);

	return 0;
}


static void fsp_transaction_send_loop(void *data);

static void
try_fsp_transaction(void *data)
{
	ELOG
	struct connection *conn = (struct connection *)data;
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)conn->info;
	FSP_SESSION *s = fsp->ses;

	//FD_ZERO(&mask);
	/* get the next key */

	fsp->out.key = client_get_key((FSP_LOCK *)s->lock);
	fsp->retry = random() & 0xfff8;

	if (s->seq == fsp->retry) {
		s->seq ^= 0x1080;
	} else {
		s->seq = fsp->retry;
	}
	fsp->dupes = fsp->retry = 0;
	fsp->t_delay = 0;
	/* compute initial delay here */
	/* we are using hardcoded value for now */
	fsp->f_delay = 1340;
	fsp->l_delay = 0;

	fsp_transaction_send_loop(conn);
}

static void fsp_transaction_continue(void *data);

static void
fsp_transaction_send_loop(void *data)
{
	ELOG
	struct connection *conn = (struct connection *)data;
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)conn->info;

	FSP_SESSION *s = fsp->ses;
	size_t l;

	if (fsp->t_delay >= s->timeout) {
		client_set_key((FSP_LOCK *)s->lock, fsp->out.key);
		errno = ETIMEDOUT;
		fsp->ret = -1;

		if (fsp->return_func) {
			fsp->return_func(conn);
		}
		return;
	}
	/* make a packet */
	fsp->out.key = client_get_key((FSP_LOCK *)s->lock);
	fsp->out.seq = (s->seq) | (fsp->retry & 0x7);
	l = fsp_pkt_write(&fsp->out, &fsp->buf);

	/* We should compute next delay wait time here */
	gettimeofday(&fsp->start[fsp->retry & 0x7], NULL);

	if (fsp->retry == 0) {
		fsp->w_delay = fsp->f_delay;
	} else {
		fsp->w_delay = fsp->l_delay * 3 / 2;
	}
	fsp->l_delay = fsp->w_delay;

	/* send packet */
	if (l <= 0 || sendto(s->fd, fsp->buf, l, 0, (struct sockaddr *)&s->host_addr, sizeof(s->host_addr)) < 0) {
#if 0
		fprintf(stderr, "Send failed errno=%d\n", errno);
#endif
		if (errno == EBADF || errno == ENOTSOCK) {
			client_set_key((FSP_LOCK *)s->lock, fsp->out.key);
			errno = EBADF;

			if (fsp->return_func) {
				fsp->return_func(conn);
			}
			return;
		}
		fsp->retry--;
		fsp->t_delay += 1000;
		install_timer(&fsp->send_timer, 1000, fsp_transaction_send_loop, conn);
		return;
	}
	/* keep delay value within sane limits */
	if (fsp->w_delay > (int)s->maxdelay) {
		fsp->w_delay = s->maxdelay;
	} else if (fsp->w_delay < 1000) {
		fsp->w_delay = 1000;
	}
	fsp->t_delay += fsp->w_delay;
	set_handlers(s->fd, fsp_transaction_continue, NULL, NULL, conn, EL_TYPE_UDP);
}

static void
fsp_transaction_continue(void *data)
{
	ELOG
	struct connection *conn = (struct connection *)data;
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)conn->info;

	FSP_SESSION *s = fsp->ses;
	ssize_t r;

	kill_timer(&fsp->send_timer);

	if (fsp->w_delay <= 0) {
		fsp->retry++;
		register_bottom_half(fsp_transaction_send_loop, conn);
		return;
	}
	/* convert w_delay to timeval */
	fsp->stop.tv_sec = fsp->w_delay / 1000;
	fsp->stop.tv_usec = (fsp->w_delay % 1000) * 1000;
	r = recv(s->fd, fsp->buf, FSP_MAXPACKET, 0);

	if (r < 0) {
		/* serious recv error */
		client_set_key((FSP_LOCK *)s->lock, fsp->out.key);

		fsp->ret = -1;
		if (fsp->return_func) {
			fsp->return_func(conn);
		}
		return;
	}
	gettimeofday(&fsp->stop, NULL);
	fsp->w_delay -= 1000 * (fsp->stop.tv_sec - fsp->start[fsp->retry & 0x7].tv_sec);
	fsp->w_delay -= (fsp->stop.tv_usec - fsp->start[fsp->retry & 0x7].tv_usec) / 1000;

	/* process received packet */
	if (fsp_pkt_read(&fsp->in, fsp->buf, r) < 0) {
		set_handlers(s->fd, fsp_transaction_continue, NULL, NULL, conn, EL_TYPE_UDP);
		/* unpack failed */
		return;
	}

	/* check sequence number */
	if ((fsp->in.seq & 0xfff8) != s->seq) {
		/* duplicate */
		fsp->dupes++;
		set_handlers(s->fd, fsp_transaction_continue, NULL, NULL, conn, EL_TYPE_UDP);
		return;
	}

	/* check command code */
	if ((fsp->in.cmd != fsp->out.cmd) && (fsp->in.cmd != FSP_CC_ERR)) {
		fsp->dupes++;
		set_handlers(s->fd, fsp_transaction_continue, NULL, NULL, conn, EL_TYPE_UDP);
		return;
	}

	/* check correct filepos */
	if ((fsp->in.pos != fsp->out.pos) && ( fsp->in.cmd == FSP_CC_GET_DIR ||
		fsp->out.cmd == FSP_CC_GET_FILE || fsp->out.cmd == FSP_CC_UP_LOAD ||
		fsp->out.cmd == FSP_CC_GRAB_FILE || fsp->out.cmd == FSP_CC_INFO) ) {
		fsp->dupes++;
		set_handlers(s->fd, fsp_transaction_continue, NULL, NULL, conn, EL_TYPE_UDP);
		return;
	}

	/* now we have a correct packet */
	/* compute rtt delay */
	fsp->w_delay = 1000 * (fsp->stop.tv_sec - fsp->start[fsp->retry & 0x7].tv_sec);
	fsp->w_delay += (fsp->stop.tv_usec -  fsp->start[fsp->retry & 0x7].tv_usec) / 1000;
	/* update last stats */
	s->last_rtt = fsp->w_delay;
	s->last_delay = fsp->f_delay;
	s->last_dupes = fsp->dupes;
	s->last_resends = fsp->retry;
	/* update cumul. stats */
	s->dupes += fsp->dupes;
	s->resends += fsp->retry;
	s->trips++;
	s->rtts += fsp->w_delay;
	/* grab a next key */
	client_set_key((FSP_LOCK *)s->lock, fsp->in.key);
	errno = 0;
	fsp->ret = 0;

	if (fsp->return_func) {
		fsp->return_func(conn);
	}
}

static int
_x_adr(const char *xhost, unsigned short port, struct sockaddr_in *his)
{
	struct hostent *H;
	int    i;
	char *s, *d;

	memset(his, 0, sizeof(*his));

#ifdef HAVE_INET_PTON
	if (inet_pton(AF_INET, xhost, &his->sin_addr)) {
#elif defined(HAVE_INET_ATON)
	if (inet_aton(xhost, &his->sin_addr)) {
#else
	his->sin_addr.s_addr = inet_addr(xhost);
	if (his->sin_addr.s_addr != INADDR_NONE) {
#endif
		his->sin_family = AF_INET;
	} else if ((H = gethostbyname(xhost))) {
		for (s = (char *)H->h_addr, d = (char *)&his->sin_addr, i = H->h_length; i--; *d++ = *s++);
		his->sin_family = H->h_addrtype;
	} else return -1;
	his->sin_port = htons((unsigned short) port);

	return 0;
}

static int
_x_udp(unsigned short *port)
{
	int f, zz = 1;
	struct sockaddr_in me ;
	struct sockaddr_in sin;
	socklen_t len;

	memset(&me, 0, sizeof(me));
	me.sin_port = htons(*port);
	me.sin_family = AF_INET;

	if ((f = socket(AF_INET,SOCK_DGRAM,0)) == -1) {
		return -1;
	}

	if (setsockopt(f, SOL_SOCKET, SO_REUSEADDR, (const char *)&zz, sizeof(zz)) < 0) {
		int tmp = errno;
		close(f);
		errno = tmp;
		return -1;
	}

	if (bind(f, (struct sockaddr *)&me, (len = sizeof(me))) < 0) {
		int tmp = errno;
		close(f);
		errno = tmp;
		return -1;
	}

	if (getsockname(f, (struct sockaddr*)&sin, &len) < 0) {
		int tmp = errno;
		close(f);
		errno = tmp;
		return -1;
	}
	if (!*port) *port = ntohs((unsigned short)sin.sin_port);
	return f;
}

static int
init_client(const char *myhost, unsigned short port, unsigned short myport, struct sockaddr_in *server_addr)
{
	int myfd;

	if ((myfd = _x_udp(&myport)) == -1) {
		return -1;
	}

	if (_x_adr(myhost, port, server_addr) == -1) {
		close(myfd);
		return -1;
	}

	return myfd;
}

/* ******************* Session management functions ************ */

/* initializes a session */
static FSP_SESSION *
fsp_open_session(const char *host, unsigned short port, const char *password)
{
	ELOG
	FSP_SESSION *s;
	int fd;
	struct sockaddr_in addrin;

	FSP_LOCK *lock;

	fd = init_client(host, port, 0, &addrin);

	if (fd < 0) {
		return NULL;
	}

	/* allocate memory */
	s = mem_calloc(1, sizeof(*s));

	if (!s) {
		close(fd);
		errno = ENOMEM;
		return NULL;
	}
	lock = mem_calloc(1, sizeof(*lock));

	if (!lock) {
		close(fd);
		mem_free(s);
		errno = ENOMEM;
		return NULL;
	}
	s->lock = lock;
	/* init locking subsystem */

	if (client_init_key((FSP_LOCK *)s->lock, addrin.sin_addr.s_addr, ntohs(addrin.sin_port))) {
		mem_free(s);
		close(fd);
		mem_free(lock);
		return NULL;
	}
	s->fd = fd;
	s->timeout = 300000; /* 5 minutes */
	s->maxdelay = 60000; /* 1 minute  */
	s->seq = random() & 0xfff8;

	if (password) {
		s->password = stracpy(password);
	}
	s->host_addr = addrin;

	return s;
}

static void bye_bye(void *data);

/* closes a session */
static void
try_fsp_close_session(struct connection *conn)
{
	ELOG
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)conn->info;

	if (!fsp->ses || fsp->ses->fd == -1) {
		return;
	}
	fsp->return_func = bye_bye;
	fsp->out.cmd = FSP_CC_BYE;
	fsp->out.len = fsp->out.xlen = 0;
	fsp->out.pos = 0;
	fsp->ses->timeout = 7000;

	try_fsp_transaction(conn);
	clear_handlers(fsp->ses->fd);
	bye_bye(conn);
}

static void
bye_bye(void *data)
{
	ELOG
	struct connection *conn = (struct connection *)data;
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)conn->info;
	FSP_SESSION *s = fsp->ses;

	kill_timer(&fsp->send_timer);
	close(s->fd);
	mem_free_if(s->password);
	client_destroy_key((FSP_LOCK *)s->lock);
	mem_free(s->lock);
	memset(s, 0, sizeof(FSP_SESSION));
	s->fd = -1;
	mem_free(s);

	fsp->return_func = NULL;
}

/* *************** Directory listing functions *************** */

/* native FSP directory reader */
static int
fsp_readdir_native(FSP_DIR *dir, FSP_RDENTRY *entry, FSP_RDENTRY **result)
{
	ELOG
	unsigned char ftype;
	int namelen;

	if (dir == NULL || entry == NULL || result == NULL) {
		return -EINVAL;
	}

	if (dir->dirpos < 0 || dir->dirpos % 4) {
		return -ESPIPE;
	}

	while(1) {
		if (dir->dirpos >= (int)dir->datasize) {
			/* end of the directory */
			*result = NULL;
			return 0;
		}

		if (dir->blocksize - (dir->dirpos % dir->blocksize) < 9) {
			ftype = FSP_RDTYPE_SKIP;
		} else {
			/* get the file type */
			ftype = dir->data[dir->dirpos + 8];
		}

		if (ftype == FSP_RDTYPE_END) {
			dir->dirpos = dir->datasize;
			continue;
		}

		if (ftype == FSP_RDTYPE_SKIP) {
			/* skip to next directory block */
			dir->dirpos = (dir->dirpos / dir->blocksize + 1) * dir->blocksize;
#ifdef MAINTAINER_MODE
			fprintf(stderr, "new block dirpos: %d\n", dir->dirpos);
#endif
			continue;
		}
		/* extract binary data */
		entry->lastmod = ntohl(*(const uint32_t *)(dir->data + dir->dirpos));
		entry->size = ntohl(*(const uint32_t *)(dir->data + dir->dirpos + 4));
		entry->type = ftype;
		/* skip file date and file size */
		dir->dirpos += 9;
		/* read file name */
		entry->name[255] = '\0';
		strncpy(entry->name, (char *)(dir->data + dir->dirpos), 255);
		/* check for ASCIIZ encoded filename */
		if (memchr(dir->data + dir->dirpos, 0, dir->datasize - dir->dirpos) != NULL) {
			namelen = strlen((char *) dir->data+dir->dirpos);
		} else {
			/* \0 terminator not found at end of filename */
			*result = NULL;
			return 0;
		}
		/* skip over file name */
		dir->dirpos += namelen + 1;
		/* set entry namelen field */
		if (namelen > 255) {
			entry->namlen = 255;
		} else {
			entry->namlen = namelen;
		}
		/* set record length */
		entry->reclen = 10 + namelen;

		/* pad to 4 byte boundary */
		while (dir->dirpos & 0x3) {
			dir->dirpos++;
			entry->reclen++;
		}
		/* and return it */
		*result = entry;

		return 0;
	}
}

static int
fsp_closedir(FSP_DIR *dirp)
{
	ELOG
	if (dirp == NULL) {
		return -1;
	}

	mem_free_if(dirp->dirname);
	mem_free(dirp->data);
	mem_free(dirp);

	return 0;
}

/*  ***************** File input/output functions *********  */
static FSP_FILE *
fsp_fopen(struct connection *conn, const char *path, const char *modeflags)
{
	ELOG
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)conn->info;
	FSP_SESSION *session = fsp->ses;
	FSP_FILE   *f;

	if (session == NULL || path == NULL || modeflags == NULL) {
		errno = EINVAL;
		return NULL;
	}

	f = mem_calloc(1, sizeof(*f));

	if (f == NULL) {
		return NULL;
	}

	/* check and parse flags */
	switch (*modeflags++) {
	case 'r':
		break;
	default:
		mem_free(f);
		errno = EINVAL;
		 return NULL;
	}

	if (*modeflags == '+' || ( *modeflags == 'b' && modeflags[1] == '+')) {
		mem_free(f);
		return NULL;
	}

	/* build request packet */
	if (0 && f->writing) {
		f->out.cmd = FSP_CC_UP_LOAD;
	} else {
		if (buildfilename(session, &fsp->out, path)) {
			mem_free(f);
			return NULL;
		}
		f->bufpos = FSP_SPACE;
		fsp->out.cmd = FSP_CC_GET_FILE;
	}
	fsp->out.xlen = 0;
	/* setup control variables */
	f->s = session;
	f->name = stracpy(path);

	if (f->name == NULL) {
		mem_free(f);
		errno = ENOMEM;
		return NULL;
	}

	return f;
}

static int
fsp_fclose(FSP_FILE *file)
{
	ELOG
	int rc;

	rc = 0;
	errno = 0;
	mem_free(file->name);
	mem_free(file);

	return rc;
}


static void
try_fsp_stat(struct connection *conn, const char *path)
{
	ELOG
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)conn->info;

	if (buildfilename(fsp->ses, &fsp->out, path)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	fsp->path = stracpy(path);

	if (!fsp->path) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	memset(&fsp->sb, 0xAA, sizeof(fsp->sb));

	fsp->out.cmd = FSP_CC_STAT;
	fsp->out.xlen = 0;
	fsp->out.pos = 0;
	fsp->return_func = fsp_stat_continue;

	try_fsp_transaction(conn);
}

static void try_fsp_read_file(void *data);
static void try_fsp_opendir(void *data);

static void
fsp_stat_continue(void *data)
{
	ELOG
	struct connection *conn = (struct connection *)data;
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)conn->info;
	unsigned char ftype;

	if (fsp->ret) {
		if (errno) {
			abort_connection(conn, connection_state_for_errno(errno));
		} else {
			abort_connection(conn, connection_state(S_FSP_OPEN_SESSION_UNKN));
		}
		return;
	}

	if (fsp->in.cmd == FSP_CC_ERR) {
		errno = ENOENT;
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}
	/* parse results */
	ftype = fsp->in.buf[8];

	if (ftype == 0) {
		errno = ENOENT;
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}
	fsp->sb.st_uid = fsp->sb.st_gid = 0;
	fsp->sb.st_mtime = fsp->sb.st_ctime = fsp->sb.st_atime = ntohl(*(const uint32_t *)(fsp->in.buf));
	fsp->sb.st_size = ntohl(*(const uint32_t *)(fsp->in.buf + 4));
	//fsp->sb.st_blocks = (fsp->sb.st_size + 511) / 512;

	if (ftype == FSP_RDTYPE_DIR) {
		fsp->sb.st_mode = S_IFDIR | 0755;
		fsp->sb.st_nlink = 2;
	} else {
		fsp->sb.st_mode = S_IFREG | 0644;
		fsp->sb.st_nlink = 1;
	}
	errno = 0;

#if SIZEOF_OFF_T >= 8
	if (fsp->sb.st_size < 0 || fsp->sb.st_size > 0xFFFFFFFF) {
		/* Probably a _FILE_OFFSET_BITS mismatch as
		 * described above.  Try to detect which half
		 * of st_size is the real size.  This may
		 * depend on the endianness of the processor
		 * and on the padding in struct stat.  */
		if ((fsp->sb.st_size & 0xFFFFFFFF00000000ULL) == 0xAAAAAAAA00000000ULL) {
			fsp->sb.st_size = fsp->sb.st_size & 0xFFFFFFFF;
		} else if ((fsp->sb.st_size & 0xFFFFFFFF) == 0xAAAAAAAA) {
			fsp->sb.st_size = (fsp->sb.st_size >> 32) & 0xFFFFFFFF;
		} else	{
			/* Can't figure it out. */
			fsp->sb.st_size = 1;
		}
	}
#endif

	if (S_ISDIR(fsp->sb.st_mode)) {
		if (buildfilename(fsp->ses, &fsp->out, fsp->path)) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
		fsp->pos = 0;
		fsp->blocksize = 0;
		fsp->dir = NULL;
		fsp->out.cmd = FSP_CC_GET_DIR;
		fsp->out.xlen = 0;
		fsp->out.pos = 0;
		fsp->return_func = try_fsp_opendir;
		try_fsp_transaction(conn);
	} else {
		FSP_FILE *file = fsp_fopen(conn, fsp->path, "r");

		if (!file) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
		conn->est_length = fsp->sb.st_size;
		fsp->f = file;
		fsp->out.pos = conn->from;
		fsp->return_func = try_fsp_read_file;
		try_fsp_transaction(conn);
	}
}

static void
display_entry(struct connection *conn, const FSP_RDENTRY *fentry, const char dircolor[])
{
	ELOG
	struct string string;

	/* fentry->name is a fixed-size array and is followed by other
	 * members; thus, if the name reported by the server does not
	 * fit in the array, fsplib must either truncate or reject it.
	 * If fsplib truncates the name, it does not document whether
	 * fentry->namlen is the original length or the truncated
	 * length.  ELinks therefore ignores fentry->namlen and
	 * instead measures the length on its own.  */
	const size_t namelen = strlen(fentry->name);

	if (!init_string(&string)) return;
	add_format_to_string(&string, "%10u\t", fentry->size);

#ifdef HAVE_STRFTIME
	if (fentry->lastmod > 0) {
		time_t current_time = time(NULL);
		time_t when = (time_t)fentry->lastmod;
		struct tm *when_tm;
		char *fmt;
		/* LC_TIME=fi_FI.UTF_8 can generate "elo___ 31 23:59"
		 * where each _ denotes U+00A0 encoded as 0xC2 0xA0,
		 * thus needing a 19-byte buffer.  */
		char date[MAX_STR_LEN];
		int wr;

		when_tm = localtime(&when);

		if (current_time > when + 6L * 30L * 24L * 60L * 60L
		    || current_time < when - 60L * 60L)
			fmt = gettext("%b %e  %Y");
		else
			fmt = gettext("%b %e %H:%M");

		wr = strftime(date, sizeof(date), fmt, when_tm);
		add_cp_html_to_string(&string, get_cp_index("System"), date, wr);
	} else
#endif
	add_to_string(&string, gettext("             "));

	add_to_string(&string, "\t<a href=\"");
	/* The result of encode_uri_string does not include '&' or '<'
	 * which could mess up the HTML.  */
	encode_uri_string(&string, fentry->name, namelen, 0);
	if (fentry->type == FSP_RDTYPE_DIR) {
		add_to_string(&string, "/\">");
		if (*dircolor) {
			add_to_string(&string, "<font color=\"");
			add_to_string(&string, dircolor);
			add_to_string(&string, "\"><b>");
		}
		add_html_to_string(&string, fentry->name, namelen);
		if (*dircolor) {
			add_to_string(&string, "</b></font>");
		}
	} else {
		add_to_string(&string, "\">");
		add_html_to_string(&string, fentry->name, namelen);
	}
	add_to_string(&string, "</a>\n");
	add_fragment(conn->cached, conn->from, string.source, string.length);
	conn->from += string.length;
	done_string(&string);
}

static int
compare(const void *av, const void *bv)
{
	ELOG
	const FSP_RDENTRY *a = (const FSP_RDENTRY *)av, *b = (const FSP_RDENTRY *)bv;
	int res = ((b->type == FSP_RDTYPE_DIR) - (a->type == FSP_RDTYPE_DIR));

	if (res)
		return res;
	return strcmp(a->name, b->name);
}

static void
sort_and_display_entries(struct connection *conn, FSP_DIR *dir, const char dircolor[])
{
	ELOG
	/* fsp_readdir_native in fsplib 0.9 and earlier requires
	 * the third parameter to point to a non-null pointer
	 * even though it does not dereference that pointer
	 * and overwrites it with another one anyway.
	 * http://sourceforge.net/tracker/index.php?func=detail&aid=1875210&group_id=93841&atid=605738
	 * Work around the bug by using non-null &tmp.
	 * Nothing will actually read or write tmp.  */
	FSP_RDENTRY fentry, tmp, *table = NULL;
	FSP_RDENTRY *fresult = &tmp;
	int size = 0;
	int i;

	while (!fsp_readdir_native(dir, &fentry, &fresult)) {
		FSP_RDENTRY *new_table;

		if (!fresult) break;
		if (!strcmp(fentry.name, "."))
			continue;
		new_table = (FSP_RDENTRY *)mem_realloc(table, (size + 1) * sizeof(*table));
		if (!new_table)
			continue;
		table = new_table;
		copy_struct(&table[size], &fentry);
		size++;
	}
	/* If size==0, then table==NULL.  According to ISO/IEC 9899:1999
	 * 7.20.5p1, the NULL must not be given to qsort.  */
	if (size > 0)
		qsort(table, size, sizeof(*table), compare);

	for (i = 0; i < size; i++) {
		display_entry(conn, &table[i], dircolor);
	}
}

static void
show_fsp_directory(struct connection *conn)
{
	ELOG
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)conn->info;
	struct string buf;

	fsp->dir->inuse = 1;
	fsp->dir->blocksize = fsp->blocksize;
	fsp->dir->dirname = stracpy(fsp->path);
	fsp->dir->datasize = fsp->pos;

	errno = 0;
	char *data = get_uri_string(conn->uri, URI_DATA);
	char dircolor[8] = "";

	if (!conn->cached) conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (!data) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	decode_uri(data);

	if (!is_in_state(init_directory_listing(&buf, conn->uri), S_OK)) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	add_fragment(conn->cached, conn->from, buf.source, buf.length);
	conn->from += buf.length;
	done_string(&buf);

	if (get_opt_bool("document.browse.links.color_dirs", NULL)) {
		color_to_string(get_opt_color("document.colors.dirs", NULL),
				dircolor);
	}

	sort_and_display_entries(conn, fsp->dir, dircolor);
//	fsp_closedir(dir);
	add_fragment(conn->cached, conn->from, "</pre><hr/></body></html>", strlen("</pre><hr/></body></html>"));
	conn->from += strlen("</pre><hr/></body></html>");
//	fsp_close_session(ses);
	mem_free_set(&conn->cached->content_type, stracpy("text/html"));
	abort_connection(conn, connection_state(S_OK));

	mem_free(data);
}

static void
try_fsp_opendir(void *data)
{
	ELOG
	struct connection *conn = (struct connection *)data;
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)conn->info;
	unsigned char *tmp;

	if (fsp->ret) {
		if (errno) {
			abort_connection(conn, connection_state_for_errno(errno));
		} else {
			abort_connection(conn, connection_state(S_FSP_OPEN_SESSION_UNKN));
		}
		return;
	}

	if (fsp->in.cmd == FSP_CC_ERR) {
		errno = EIO;
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}

	if (!fsp->in.len) {
		show_fsp_directory(conn);
		return;
	}

	if (fsp->blocksize == 0) {
		fsp->blocksize = fsp->in.len;
	}

	if (fsp->dir == NULL) {
		fsp->dir = mem_calloc(1, sizeof(*fsp->dir));

		if (!fsp->dir) {
			abort_connection(conn, connection_state(S_OUT_OF_MEM));
			return;
		}
	}
	tmp = mem_realloc(fsp->dir->data, fsp->pos + fsp->in.len);

	if (tmp == NULL) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	fsp->dir->data = tmp;
	memcpy(fsp->dir->data + fsp->pos, fsp->in.buf, fsp->in.len);
	fsp->pos += fsp->in.len;

	if (fsp->in.len < fsp->blocksize) {
		show_fsp_directory(conn);
		return;
	}
	fsp->out.pos = fsp->pos;
	register_bottom_half(try_fsp_transaction, conn);
}

static void
try_fsp_read_file(void *data)
{
	ELOG
	struct connection *conn = (struct connection *)data;
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)conn->info;

	if (fsp->ret) {
		if (errno) {
			abort_connection(conn, connection_state_for_errno(errno));
		} else {
			abort_connection(conn, connection_state(S_FSP_OPEN_SESSION_UNKN));
		}
		return;
	}

	if (fsp->in.cmd == FSP_CC_ERR) {
		errno = EIO;
		abort_connection(conn, connection_state_for_errno(errno));
		return;
	}

	if (!conn->cached) conn->cached = get_cache_entry(conn->uri);
	if (!conn->cached) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}

	if (!fsp->in.len) {
		abort_connection(conn, connection_state(S_OK));
		return;
	}

	if (add_fragment(conn->cached, conn->from, (const char *)fsp->in.buf, fsp->in.len) == 1) {
		conn->tries = 0;
	}
	conn->from += fsp->in.len;
	conn->received += fsp->in.len;
	fsp->out.pos = conn->from;

	register_bottom_half(try_fsp_transaction, conn);
}

static void
done_fsp_connection(struct connection *conn)
{
	ELOG
	struct fsp_connection_info *fsp;

	if (!conn || !conn->info) {
		return;
	}
	fsp = (struct fsp_connection_info *)conn->info;

	if (fsp->dir) {
		fsp_closedir(fsp->dir);
	}

	if (fsp->f) {
		fsp_fclose(fsp->f);
	}

	if (fsp->ses) {
		try_fsp_close_session(conn);
	}
	mem_free_if(fsp->path);
}

static void
do_fsp(struct connection *conn)
{
	ELOG
	FSP_SESSION *ses;
	struct uri *uri = conn->uri;
	struct auth_entry *auth;
	char *host = get_uri_string(uri, URI_HOST);
	char *data = get_uri_string(uri, URI_DATA);
	unsigned short port = (unsigned short)get_uri_port(uri);
	char *password = NULL;
	struct fsp_connection_info *fsp = (struct fsp_connection_info *)mem_calloc(1, sizeof(*fsp));

	if (!fsp) {
		abort_connection(conn, connection_state(S_OUT_OF_MEM));
		return;
	}
	conn->info = fsp;
	conn->done = done_fsp_connection;

	decode_uri(data);
	if (uri->passwordlen) {
		password = get_uri_string(uri, URI_PASSWORD);
	} else {
		auth = find_auth(uri);
		if (auth) password = auth->password;
	}

	/* fsp_open_session may not set errno if getaddrinfo fails
	 * https://sourceforge.net/tracker/index.php?func=detail&aid=2036798&group_id=93841&atid=605738
	 * Try to detect this bug and use an ELinks-specific error
	 * code instead, so that we can display a message anyway.  */
	errno = 0;
	ses = fsp_open_session(host, port, password);

	if (!ses) {
		if (errno) {
			abort_connection(conn, connection_state_for_errno(errno));
			return;
		}
		else {
			abort_connection(conn, connection_state(S_FSP_OPEN_SESSION_UNKN));
			return;
		}
	}
	fsp->ses = ses;
	set_connection_state(conn, connection_state(S_TRANS));
	try_fsp_stat(conn, data);
}

void
fsp_protocol_handler(struct connection *conn)
{
	ELOG
	conn->from = 0;
	conn->unrestartable = 1;
	do_fsp(conn);
}
