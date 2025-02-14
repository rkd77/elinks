top_builddir=../..
include $(top_builddir)/Makefile.config

SUBDIRS-$(CONFIG_BITTORRENT)	+= bittorrent
SUBDIRS-$(CONFIG_LIBCURL)	+= curl
SUBDIRS-$(CONFIG_FINGER)	+= finger
SUBDIRS-$(CONFIG_FSP)		+= fsp
SUBDIRS-$(CONFIG_FTP)		+= ftp
SUBDIRS-$(CONFIG_GEMINI)	+= gemini
SUBDIRS-$(CONFIG_GOPHER)	+= gopher
SUBDIRS-$(CONFIG_NNTP)		+= nntp
SUBDIRS-$(CONFIG_SMB)		+= smb
SUBDIRS-$(CONFIG_SPARTAN)	+= spartan
SUBDIRS-$(CONFIG_URI_REWRITE)	+= rewrite

SUBDIRS = auth file http test

OBJS-$(CONFIG_DATA)		+= data.o

OBJS-$(CONFIG_FTP)		+= ftpparse.o
OBJS-$(CONFIG_SFTP)		+= ftpparse.o

OBJS = about.o common.o date.o header.o protocol.o proxy.o uri.o user.o

include $(top_srcdir)/Makefile.lib
