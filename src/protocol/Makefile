path_to_top=../..
include $(path_to_top)/Makefile.config

SUBDIRS-$(CONFIG_BITTORRENT)	+= bittorrent
SUBDIRS-$(CONFIG_FINGER)	+= finger
SUBDIRS-$(CONFIG_FTP)		+= ftp
SUBDIRS-$(CONFIG_GOPHER)	+= gopher
SUBDIRS-$(CONFIG_NNTP)		+= nntp
SUBDIRS-$(CONFIG_SMB)		+= smb
SUBDIRS-$(CONFIG_URI_REWRITE)	+= rewrite

OBJS-$(CONFIG_DATA)		+= data.o

SUBDIRS = auth file http

OBJS = about.o $(dataobj) date.o header.o protocol.o proxy.o uri.o user.o

include $(path_to_top)/Makefile.lib
