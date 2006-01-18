top_builddir=../..
include $(top_builddir)/Makefile.config

SUBDIRS = auth file http

SUBDIRS-$(CONFIG_BITTORRENT)	+= bittorrent
SUBDIRS-$(CONFIG_FINGER)	+= finger
SUBDIRS-$(CONFIG_FSP)		+= fsp
SUBDIRS-$(CONFIG_FTP)		+= ftp
SUBDIRS-$(CONFIG_GOPHER)	+= gopher
SUBDIRS-$(CONFIG_NNTP)		+= nntp
SUBDIRS-$(CONFIG_SMB)		+= smb
SUBDIRS-$(CONFIG_URI_REWRITE)	+= rewrite

OBJS = about.o date.o header.o protocol.o proxy.o uri.o user.o

OBJS-$(CONFIG_DATA)		+= data.o
OBJS-$(CONFIG_CGI)		+= common.o
OBJS-$(CONFIG_FSP)		+= common.o
OBJS-$(CONFIG_SMB)		+= common.o

include $(top_srcdir)/Makefile.lib
