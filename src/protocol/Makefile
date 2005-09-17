path_to_top=../..
include $(path_to_top)/Makefile.config

authobj = auth/auth.o auth/dialogs.o auth/digest.o

ifeq ($(CONFIG_CGI),yes)
cgiobj = file/cgi.o
endif

ifeq ($(CONFIG_BITTORRENT),yes)
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

ifeq ($(CONFIG_DATA),yes)
dataobj = data.o
endif

fileobj = file/file.o

ifeq ($(CONFIG_FINGER),yes)
fingerdir = finger
fingerobj = $(fingerdir)/finger.o
endif

ifeq ($(CONFIG_FTP),yes)
ftpdir = ftp
ftpobj = $(ftpdir)/ftp.o $(ftpdir)/parse.o 
endif

ifeq ($(CONFIG_GOPHER),yes)
gopherdir = gopher
gopherobj = $(gopherdir)/gopher.o
endif

httpobj = http/blacklist.o http/codes.o http/http.o

ifeq ($(CONFIG_NNTP),yes)
nntpdir = nntp
nntpobj = $(nntpdir)/connection.o $(nntpdir)/nntp.o $(nntpdir)/response.o
endif

ifeq ($(CONFIG_SMB),yes)
smbdir = smb
smbobj = $(smbdir)/smb.o
endif

ifeq ($(CONFIG_URI_REWRITE),yes)
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

SUB_OBJS = \
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
OBJS = about.o date.o header.o protocol.o proxy.o uri.o user.o $(SUB_OBJS)

all-l: libprotocol.a
libprotocol.a: $(OBJS)

include $(path_to_top)/Makefile.lib
