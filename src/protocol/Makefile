path_to_top=../..
include $(path_to_top)/Makefile.config

ifeq ($(CONFIG_BITTORRENT),yes)
bittorrentdir = bittorrent
endif

ifeq ($(CONFIG_DATA),yes)
dataobj = data.o
endif

ifeq ($(CONFIG_FINGER),yes)
fingerdir = finger
endif

ifeq ($(CONFIG_FTP),yes)
ftpdir = ftp
endif

ifeq ($(CONFIG_GOPHER),yes)
gopherdir = gopher
endif

ifeq ($(CONFIG_NNTP),yes)
nntpdir = nntp
endif

ifeq ($(CONFIG_SMB),yes)
smbdir = smb
endif

ifeq ($(CONFIG_URI_REWRITE),yes)
rewritedir = rewrite
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

OBJS = about.o $(dataobj) date.o header.o protocol.o proxy.o uri.o user.o

include $(path_to_top)/Makefile.lib
