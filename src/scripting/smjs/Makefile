top_builddir=../../..
include $(top_builddir)/Makefile.config

INCLUDES += $(SPIDERMONKEY_CFLAGS)

OBJS = smjs.o core.o global_object.o hooks.o elinks_object.o cache_object.o \
       view_state_object.o action_object.o bookmarks.o globhist.o \
       keybinding.o load_uri.o session_object.o terminal_object.o

include $(top_srcdir)/Makefile.lib
