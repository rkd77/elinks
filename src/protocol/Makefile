path_to_top=../..
include $(path_to_top)/Makefile.config

authobj = auth/lib.o

ifeq ($(CONFIG_BITTORRENT),yes)
bittorrentdir = bittorrent
bittorrentobj = $(bittorrentdir)/lib.o
endif

ifeq ($(CONFIG_DATA),yes)
dataobj = data.o
endif

fileobj = file/lib.o

ifeq ($(CONFIG_FINGER),yes)
fingerdir = finger
fingerobj = $(fingerdir)/lib.o
endif

ifeq ($(CONFIG_FTP),yes)
ftpdir = ftp
ftpobj = $(ftpdir)/lib.o
endif

ifeq ($(CONFIG_GOPHER),yes)
gopherdir = gopher
gopherobj = $(gopherdir)/lib.o
endif

httpobj = http/lib.o

ifeq ($(CONFIG_NNTP),yes)
nntpdir = nntp
nntpobj = $(nntpdir)/lib.o
endif

ifeq ($(CONFIG_SMB),yes)
smbdir = smb
smbobj = $(smbdir)/lib.o
endif

ifeq ($(CONFIG_URI_REWRITE),yes)
rewritedir = rewrite
rewriteobj = $(rewritedir)/lib.o
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

include $(path_to_top)/Makefile.lib
