## Process this file with automake to produce Makefile.in
## $Id: Makefile.am,v 1.48 2005/07/11 10:59:04 jonas Exp $

include $(top_srcdir)/Makefile.base

authobj = auth/auth.o auth/dialogs.o auth/digest.o

if CONFIG_CGI
cgiobj = file/cgi.o
endif

if CONFIG_BITTORRENT
bittorrentdir = bittorrent
bittorrentobj = \
 $(bittorrentdir)/bencoding.o \
 $(bittorrentdir)/bittorrent.o \
 $(bittorrentdir)/common.o \
 $(bittorrentdir)/connection.o \
 $(bittorrentdir)/dialogs.o \
 $(bittorrentdir)/peerconnect.o \
 $(bittorrentdir)/peerwire.o \
 $(bittorrentdir)/piececache.o \
 $(bittorrentdir)/tracker.o
endif

if CONFIG_DATA
dataobj = data.o
endif

fileobj = file/file.o

if CONFIG_FINGER
fingerdir = finger
fingerobj = $(fingerdir)/finger.o
endif

if CONFIG_FTP
ftpdir = ftp
ftpobj = $(ftpdir)/ftp.o $(ftpdir)/parse.o 
endif

if CONFIG_GOPHER
gopherdir = gopher
gopherobj = $(gopherdir)/gopher.o
endif

httpobj = http/blacklist.o http/codes.o http/http.o

if CONFIG_NNTP
nntpdir = nntp
nntpobj = $(nntpdir)/connection.o $(nntpdir)/nntp.o $(nntpdir)/response.o
endif

if CONFIG_SMB
smbdir = smb
smbobj = $(smbdir)/smb.o
endif

if CONFIG_URI_REWRITE
rewritedir = rewrite
rewriteobj = $(rewritedir)/rewrite.o
endif

SUBDIRS = \
 auth \
 $(bittorrentdir) \
 file \
 $(fingerdir) \
 $(ftpdir) \
 $(gopherdir) \
 http \
 $(nntpdir) \
 $(rewritedir) \
 $(smbdir)

noinst_LIBRARIES = libprotocol.a

EXTRA_libprotocol_a_SOURCES = data.c data.h
libprotocol_a_LIBADD = \
 $(authobj) \
 $(bittorrentobj) \
 $(dataobj) \
 $(cgiobj) \
 $(fileobj) \
 $(fingerobj) \
 $(ftpobj) \
 $(gopherobj) \
 $(httpobj) \
 $(nntpobj) \
 $(rewriteobj) \
 $(smbobj)

libprotocol_a_SOURCES = \
 about.c \
 about.h \
 date.c \
 date.h \
 header.c \
 header.h \
 protocol.c \
 protocol.h \
 proxy.c \
 proxy.h \
 uri.c \
 uri.h \
 user.c \
 user.h
